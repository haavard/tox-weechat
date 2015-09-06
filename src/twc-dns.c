/*
 * Copyright (c) 2015 Håvard Pettersson <mail@haavard.me>,
 *                    Michael Raitza <spacefrogg-devel@meterriblecrew.net>
 *
 * This file is part of Tox-WeeChat.
 *
 * Tox-WeeChat is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Tox-WeeChat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Tox-WeeChat.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>

#include <ldns/ldns.h>
#undef dprintf /* well, yeah, well... */

#include "twc.h"
#include "twc-utils.h"

#include "twc-dns.h"

/**
 * Struct for holding information about a Tox DNS query callback.
 */
struct t_twc_dns_callback_info
{
    void (*callback)(void *data,
                     enum t_twc_dns_rc rc,
                     const uint8_t *tox_id);
    void *data;

    struct t_hook *hook;
};

/**
 * Struct holding the split dns_id.
 */
typedef struct
{
    char *name;
    char *domain;
} t_twc_toxdns_data;

/**
 * Struct holding a list of DNS responses.
 */
typedef struct
{
    size_t count;
    uint8_t **data;  
} t_resolve_response_list;

static t_twc_toxdns_data *
toxdns_data_new(size_t namelen, size_t domlen)
{
    t_twc_toxdns_data *d;
    d = (t_twc_toxdns_data *)malloc(sizeof(t_twc_toxdns_data)
				    + namelen + domlen + 2);
    if (!d)
	return NULL;
    d->name = (char *)malloc(namelen + 1);
    if (!d->name)
    {
	free(d);
	return NULL;
    }
    d->domain = (char *)malloc(domlen + 1);
    if (!d->domain)
    {
	free(d);
	free(d->name);
	return NULL;
    }
    return d;
}

static void
toxdns_data_deep_free(t_twc_toxdns_data *d)
{
    if (d)
    {
	if (d->name)
	    free(d->name);
	if (d->domain)
	    free(d->domain);
	free(d);
    }
}

/**
 * Splits [tox:]<name>[@|._tox.]<domain> into a struct S with two
 * fields S.name = "<name>" and S.domain = "_tox.<domain>.". Note the
 * ending dot that finalises the DNS domain for later query!
 *
 * name: a string that is shorter than or equal to RESOLVE_ID_MAXLEN
 *       and must be in the form mentioned above.
 */
static t_twc_toxdns_data *
split_name(const char *name, enum t_twc_dns_rc *error)
{
    enum t_twc_dns_rc err = TWC_DNS_RC_OK;
    t_twc_toxdns_data *data = NULL;
    uint32_t name_len = strlen(name);
    char n[name_len + 1];
    char d[name_len + 6];
    char *domain;
    char *d_pos = d;
    char *n_pos = n;

    strcpy(n, name);
    domain = strstr(n, "tox:");  /* ignore a "tox:" uri prefix */
    if (domain && (domain == n))
	n_pos = &n[4];

    domain = strstr(n_pos, "._tox.");
    if ((!domain) || (domain == n_pos) || (strlen(domain) <= 6))
    {
	domain = strstr(n_pos, "@");
	if ((!domain) || (domain == n_pos) || (strlen(domain) <= 1))
	{
	    err = TWC_DNS_RC_EINVAL;
	    goto err;
	}
	d_pos = stpcpy(d_pos, "_tox.");
    }
    d_pos = stpcpy(d_pos, &domain[1]);
    strcpy(d_pos, ".");
    domain[0] = 0;

    data = toxdns_data_new(strlen(n_pos), strlen(d));
    if (!data)
    {
	err = TWC_DNS_RC_ERROR;
	goto err;
    }
    strcpy(data->name, n_pos);
    strcpy(data->domain, d);

  err:
    *error = err;
    return data;
}

static size_t
resolve_response_count(const t_resolve_response_list *resp)
{
    if (resp)
	return resp->count;
    return 0;
}

static uint8_t *
resolve_response_data(const t_resolve_response_list *resp, const size_t nr)
{
    if (nr < resolve_response_count(resp))
	return resp->data[nr];
    return NULL;
}

static t_resolve_response_list *
resolve_response_list_new(size_t size)
{
    t_resolve_response_list *resps;
    resps = (t_resolve_response_list *)malloc(sizeof(t_resolve_response_list)
					      + sizeof(char*) * size);
    if (!resps)
	return NULL;
    resps->count = 0;
    resps->data = (uint8_t **)&resps->data + 1;
    return resps;
}

static void
resolve_response_list_deep_free(t_resolve_response_list *resp)
{
    if (!resp)
	return;
    for (size_t i = 0; i < resolve_response_count(resp); ++i)
	if (resp->data[i])
	    free(resp->data[i]);
    free(resp);
}

static ldns_rr_list *
twc_ldns_query(ldns_resolver* res, const char *name, const ldns_rr_type t,
	       enum t_twc_dns_rc *err)
{
    ldns_rdf *domain = ldns_dname_new_frm_str(name);
    ldns_pkt *pkt;
    ldns_rr_list *txt = NULL;

    *err = TWC_DNS_RC_ERROR;

    if (!domain)
	goto err_dom;

    for (size_t i = 0; i < ldns_resolver_nameserver_count(res); ++i)
    {
        pkt = ldns_resolver_query(res, domain, t, LDNS_RR_CLASS_IN, LDNS_RD);
        if (!pkt)
            goto fin;
        switch (ldns_pkt_get_rcode(pkt))
        {
            case LDNS_RCODE_NOERROR:
                *err = TWC_DNS_RC_OK;
                txt = ldns_pkt_rr_list_by_type(pkt, t, LDNS_SECTION_ANSWER);
                goto fin;
            case LDNS_RCODE_REFUSED:
            case LDNS_RCODE_NXDOMAIN:
            case LDNS_RCODE_SERVFAIL:
                for (size_t i = 0; i < ldns_resolver_nameserver_count(res); ++i)
                    if (!ldns_rdf_compare(ldns_pkt_answerfrom(pkt),
                                          ldns_resolver_nameservers(res)[i]))
                    {
                        /* mark faulty nameserver as unreachable */
                        ldns_resolver_set_nameserver_rtt(res, i, LDNS_RESOLV_RTT_INF);
                        break;
                    }
                break;
            default:
                goto fin;
        }
    }
  fin:
    ldns_rdf_deep_free(domain);
    if (pkt)
	ldns_pkt_free(pkt);
  err_dom:
    return txt;
}

/**
 * Resolve the TXT records for NAME and return all responses. Outer
 * quotes are stripped from TXT records. All fields of a TXT record
 * are stiched together to a long string stripping white space as
 * requested by protocol.
 */
static t_resolve_response_list *
twc_ldns_resolve(char *name, enum t_twc_dns_rc *err) {
    ldns_resolver *res;
    ldns_status s;
    ldns_rr_list *txt = NULL;
    t_resolve_response_list *txts = NULL;

    *err= TWC_DNS_RC_OK;

    s = ldns_resolver_new_frm_file(&res, NULL);
    if (s != LDNS_STATUS_OK)
    {
	*err = TWC_DNS_RC_ERROR;
	goto err;
    }
    txt = twc_ldns_query(res, name, LDNS_RR_TYPE_TXT, err);
    if (!txt)
	goto err_pkt;
    txts = resolve_response_list_new(ldns_rr_list_rr_count(txt));
    if (!txts)
    {
	*err = TWC_DNS_RC_ERROR;
	goto err_txt;
    }

    for (size_t i = 0; i < ldns_rr_list_rr_count(txt); ++i)
    {
	ldns_rr *rr = ldns_rr_list_rr(txt, i);
	char txt_field[256] = {0}; /* TXT RR RDATA can be 255 bytes long */
	char *cur_pos = txt_field;

	/* stich together all TXT RR RDATA fields, stripping quotes */
	for (size_t j = 0; j < ldns_rr_rd_count(rr); ++j)
	{
	    char *rdata = ldns_rdf2str(ldns_rr_rdf(rr, j));
	    size_t len = strlen(rdata);

	    if (rdata[0] == '"')
	    {
		rdata[len - 1] = 0;
		cur_pos = stpcpy(cur_pos, &rdata[1]);
	    } else
		cur_pos = stpcpy(cur_pos, rdata);
	    free(rdata);
	}
	txts->data[i] = (uint8_t *)strdup(txt_field);
	if (!txts->data[i])
	{
	    *err = TWC_DNS_RC_ERROR;
	    goto err_resp;
	}
	++txts->count;
    }
    goto err_txt; // Normal return

  err_resp:
    resolve_response_list_deep_free(txts);
    txts = NULL;
  err_txt:
    ldns_rr_list_deep_free(txt);
  err_pkt:
    ldns_resolver_deep_free(res);
  err:
    return txts;
}

static uint8_t *
resolve_toxdns1(const t_twc_toxdns_data *data, enum t_twc_dns_rc *error)
{
    char buf[TWC_DNS_ID_MAXLEN + 2];
    t_resolve_response_list *id_rrs;
    uint8_t *toxid = NULL;
    *error = TWC_DNS_RC_OK;

    sprintf(buf, "%s.%s", data->name, data->domain);
    id_rrs = twc_ldns_resolve(buf, error);
    if (!id_rrs)
	goto err;

    for (size_t j = 0; j < id_rrs->count; ++j)
    {
	uint8_t *id_rr = resolve_response_data(id_rrs, j);
	if (strncmp("v=tox1;id=", (char *)id_rr, 10))
        {
            *error = TWC_DNS_RC_VERSION;
            continue;
        }
        toxid = (uint8_t *)strndup((char *)&id_rr[10], TOX_ADDRESS_SIZE * 2);
        if (!toxid)
        {
            *error = TWC_DNS_RC_ERROR;
            goto err_id;
        }
        *error = TWC_DNS_RC_OK;
        break;
    }

  err_id:
    resolve_response_list_deep_free(id_rrs);
  err:
    return toxid;
}

/**
 * Callback when the DNS resolver child process has written to our file
 * descriptor. Process the data a bit and pass it on to the original callback.
 */
int
twc_dns_fd_callback(void *data, int fd)
{
    struct t_twc_dns_callback_info *callback_info = data;

    char buffer[TOX_ADDRESS_SIZE * 2 + 1];
    ssize_t size = read(fd, buffer, sizeof(buffer) - 1);
    buffer[size] = '\0';

    if (size > 0)
    {
        if (size == TOX_ADDRESS_SIZE * 2)
        {
            uint8_t tox_id[TOX_ADDRESS_SIZE];
            twc_hex2bin(buffer, TOX_ADDRESS_SIZE, tox_id);

            callback_info->callback(callback_info->data,
                                    TWC_DNS_RC_OK, tox_id);
        }
        else
        {
            int rc = atoi(buffer);
            callback_info->callback(callback_info->data,
                                    rc, NULL);
        }

        weechat_unhook(callback_info->hook);
        free(callback_info);
    }

    return WEECHAT_RC_OK;
}

/**
 * Perform a Tox DNS lookup and write either a Tox ID (as a hexadecimal string)
 * or an error code (as a decimal string) to out_fd.
 */
void
twc_perform_dns_lookup(const char *dns_id, int out_fd)
{
    enum t_twc_dns_rc err = TWC_DNS_RC_OK;
    t_twc_toxdns_data *d;
    uint8_t *toxid;

    if (!dns_id || weechat_strlen_screen(dns_id) > TWC_DNS_ID_MAXLEN)
    {
	err = TWC_DNS_RC_EINVAL;
	goto err;
    }
    /* Split name from domain in preparation for tox3dns protocol. */
    d = split_name(dns_id, &err);
    
    if (!d)
    {
	err = TWC_DNS_RC_EINVAL;
	goto err;
    }

    /* For now only resolve Tox DNS version 1 addresses */
    toxid = resolve_toxdns1(d, &err);
    if (!toxid)
	goto err;

  err:
    if (err)
	dprintf(out_fd, "%d", err);
    else
	dprintf(out_fd, "%s", toxid);
    toxdns_data_deep_free(d);
}

/**
 * Perform a Tox DNS lookup. If return code (rc) parameter in callback is
 * TWC_DNS_RC_OK, tox_id contains the resolved Tox ID. For any other value,
 * something caused the Tox ID not to be found.
 *
 * If twc_dns_query returns TWC_DNS_RC_ERROR, the callback will never be
 * called.
 */
enum t_twc_dns_rc
twc_dns_query(const char *dns_id,
              void (*callback)(void *data,
                               enum t_twc_dns_rc rc,
                               const uint8_t *tox_id),
              void *callback_data)
{
    if (!callback) return TWC_DNS_RC_OK;

    int fifo[2];
    if (pipe(fifo) < 0)
        return TWC_DNS_RC_ERROR;

    pid_t pid;
    if ((pid = fork()) == -1)
    {
        return TWC_DNS_RC_ERROR;
    }
    else if (pid == 0)
    {
        twc_perform_dns_lookup(dns_id, fifo[1]);
        _exit(0);
    }
    else
    {
        struct t_twc_dns_callback_info *callback_info
            = malloc(sizeof(struct t_twc_dns_callback_info));
        if (!callback_info)
            return TWC_DNS_RC_ERROR;

        callback_info->callback = callback;
        callback_info->data = callback_data;
        callback_info->hook = weechat_hook_fd(fifo[0], 1, 0, 0,
                                              twc_dns_fd_callback,
                                              callback_info);
    }

    return TWC_DNS_RC_OK;
}

