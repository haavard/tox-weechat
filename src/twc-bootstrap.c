/*
 * Copyright (c) 2015 Håvard Pettersson <mail@haavard.me>
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

#include <tox/tox.h>

#include "twc-utils.h"

#include "twc-bootstrap.h"

char *twc_bootstrap_addresses[] = {
    "192.254.75.98",
    "31.7.57.236",
    "107.161.17.51",
    "144.76.60.215",
    "23.226.230.47",
    "37.59.102.176",
    "37.187.46.132",
    "178.21.112.187",
    "192.210.149.121",
    "54.199.139.199",
    "63.165.243.15",
};

uint16_t twc_bootstrap_ports[] = {
    33445, 443, 33445, 33445, 33445,
    33445, 33445, 33445, 33445, 33445,
    443,
};

char *twc_bootstrap_keys[] = {
    "951C88B7E75C867418ACDB5D273821372BB5BD652740BCDF623A4FA293E75D2F",
    "2A4B50D1D525DA2E669592A20C327B5FAD6C7E5962DC69296F9FEC77C4436E4E",
    "7BE3951B97CA4B9ECDDA768E8C52BA19E9E2690AB584787BF4C90E04DBB75111",
    "04119E835DF3E78BACF0F84235B300546AF8B936F035185E2A8E9E0A67C8924F",
    "A09162D68618E742FFBCA1C2C70385E6679604B2D80EA6E84AD0996A1AC8A074",
    "B98A2CEAA6C6A2FADC2C3632D284318B60FE5375CCB41EFA081AB67F500C1B0B",
    "5EB67C51D3FF5A9D528D242B669036ED2A30F8A60E674C45E7D43010CB2E1331",
    "4B2C19E924972CB9B57732FB172F8A8604DE13EEDA2A6234E348983344B23057",
    "F404ABAA1C99A9D37D61AB54898F56793E1DEF8BD46B1038B9D822E8460FAB67",
    "7F9C31FE850E97CEFD4C4591DF93FC757C7C12549DDD55F8EEAECC34FE76C029",
    "8CD087E31C67568103E8C2A28653337E90E6B8EDA0D765D57C6B5172B4F1F04C",
};

int twc_bootstrap_count = sizeof(twc_bootstrap_addresses)
                          / sizeof(twc_bootstrap_addresses[0]);

/**
 * Bootstrap a Tox object with a DHT bootstrap node. Returns the result of
 * tox_bootstrap_from_address.
 */
int
twc_bootstrap_tox(Tox *tox, const char *address, uint16_t port,
                  const char *public_key)
{
    uint8_t binary_key[TOX_FRIEND_ADDRESS_SIZE];
    twc_hex2bin(public_key, TOX_FRIEND_ADDRESS_SIZE, binary_key);

    int result = tox_bootstrap_from_address(tox, address, port,
                                            binary_key);

    return result;
}

/**
 * Bootstrap a Tox object with a random DHT bootstrap node.
 */
void
twc_bootstrap_random_node(Tox *tox)
{
    int i = rand() % twc_bootstrap_count;
    twc_bootstrap_tox(tox, twc_bootstrap_addresses[i],
                           twc_bootstrap_ports[i],
                           twc_bootstrap_keys[i]);
}

