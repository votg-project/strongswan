/*
 * Copyright (C) 2020 Vladimir Solomatin
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#define HTTP_STATUS_CODE_NOT_FOUND 404
#define HTTP_STATUS_CODE_MISDIRECTED_REQUEST 421

#include "eap_simaka_leemobi_card.h"

#include <stdio.h>
#include <ctype.h>

typedef struct private_eap_simaka_leemobi_card_t private_eap_simaka_leemobi_card_t;

void tohex(char* result, const char* data, unsigned short length) {
    unsigned short i;
    for (i = 0; i < length; i++) {
        sprintf(&result[i * 2], "%02hhX", data[i]);
    }
    result[i * 2] = 0;
}

unsigned char hexdigit(char hex) {
    return (hex <= '9') ? hex - '0' : toupper(hex) - 'A' + 10;
}

void fromhex(char* result, const char* data, unsigned short length) {
    unsigned short i, j;
    for (i = 0, j = 0; i < length; i++, j += 2) {
        result[i] = (hexdigit(data[j]) << 4) | hexdigit(data[j + 1]);
        printf("1: %02hhX\n", data[j]);
        printf("2: %02hhX\n", result[i]);
    }
}

/**
 * Private data of an eap_simaka_leemobi_card_t object.
 */
struct private_eap_simaka_leemobi_card_t {

	/**
	 * Public eap_simaka_leemobi_card_t interface.
	 */
	eap_simaka_leemobi_card_t public;

    char last_rand[AKA_RAND_LEN];
    char last_auts[AKA_AUTS_LEN];

    /**
	 * leemobi SIM url
	 */
	const char *sim_url;
};

METHOD(simaka_card_t, get_triplet, bool,
	private_eap_simaka_leemobi_card_t *this, identification_t *id,
	char rand[SIM_RAND_LEN], char sres[SIM_SRES_LEN], char kc[SIM_KC_LEN])
{
    return NOT_SUPPORTED;
}

METHOD(simaka_card_t, get_quintuplet, status_t,
	private_eap_simaka_leemobi_card_t *this, identification_t *id,
	char rand[AKA_RAND_LEN], char autn[AKA_AUTN_LEN], char ck[AKA_CK_LEN],
	char ik[AKA_IK_LEN], char res[AKA_RES_MAX], int *res_len)
{
	status_t status = FAILED;
    unsigned short response_code;
    chunk_t response = chunk_empty;
    struct json_tokener *tokener;
    json_object* response_json;
    char* url;
    char rand_hex[AKA_RAND_LEN * 2 + 1];
    char autn_hex[AKA_AUTN_LEN * 2 + 1];

    tohex(rand_hex, rand, AKA_RAND_LEN);
    tohex(autn_hex, autn, AKA_AUTN_LEN);
    if (asprintf(&url, "%s/3g-authenticate?rand=%s&autn=%s", this->sim_url, rand_hex, autn_hex) < 0) {
        return FAILED;
    }

    status = lib->fetcher->fetch(lib->fetcher, url, &response,
                                 FETCH_REQUEST_HEADER, "Accept: application/json",
                                 FETCH_RESPONSE_CODE, &response_code,
                                 FETCH_END);

    DBG1(DBG_LIB, "SIM Manager 3g-authenticate status code: %d", response_code);
    if (status != SUCCESS) {
        switch (response_code) {
            case HTTP_STATUS_CODE_NOT_FOUND:
            case HTTP_STATUS_CODE_MISDIRECTED_REQUEST:
                status = NOT_FOUND;
                break;
            default:
                status = FAILED;
                break;
        }
    }
    if (!response.ptr) {
        status = FAILED;
        DBG1(DBG_LIB, "SIM Manager 3g-authenticate response does not have a response");
    }

    if (status == SUCCESS) {
        DBG2(DBG_LIB, "SIM Manager 3g-authenticate response: %s", response.ptr);

        /* Parse HTTP response into a JSON object */
        tokener = json_tokener_new();
        response_json = json_tokener_parse_ex(tokener, response.ptr, response.len);
        json_tokener_free(tokener);
        free(response.ptr);

        if (json_object_get_type(response_json) != json_type_object) {
            status = FAILED;
        } else {
            if (!json_object_get_boolean(json_object_object_get(response_json, "synchronization"))) {
                DBG1(DBG_LIB, "SIM Manager 3g-authenticate returned synchronization failure");
                status = INVALID_STATE;
                memcpy(this->last_rand, rand, AKA_RAND_LEN);
                fromhex(this->last_auts, json_object_get_string(json_object_object_get(response_json, "auts")), AKA_AUTS_LEN);
            } else {
                DBG1(DBG_LIB, "SIM Manager 3g-authenticate returned success");
                status = SUCCESS;
                fromhex(ck, json_object_get_string(json_object_object_get(response_json, "ck")), AKA_CK_LEN);
                fromhex(ik, json_object_get_string(json_object_object_get(response_json, "ik")), AKA_IK_LEN);
                *res_len = strlen(json_object_get_string(json_object_object_get(response_json, "res")));
                fromhex(res, json_object_get_string(json_object_object_get(response_json, "res")), *res_len);
            }
        }

        json_object_put(response_json);
    } else {
        free(response.ptr);
    }

    return status;
}

METHOD(simaka_card_t, resync, bool,
       private_eap_simaka_leemobi_card_t *this, identification_t *id,
       char rand[AKA_RAND_LEN], char auts[AKA_AUTS_LEN])
{
    if (memcmp(rand, this->last_rand, AKA_RAND_LEN) != 0) {
        return false;
    } else {
        memcpy(auts, this->last_auts, AKA_AUTS_LEN);
        return true;
    }
}

METHOD(eap_simaka_leemobi_card_t, destroy, void,
	private_eap_simaka_leemobi_card_t *this)
{
    library_deinit();
	free(this);
}

/**
 * See header
 */
eap_simaka_leemobi_card_t *eap_simaka_leemobi_card_create()
{
	private_eap_simaka_leemobi_card_t *this;
	const char* sim_url;

    library_init(NULL, "eap-simaka-leemobi-card");

    sim_url = lib->settings->get_str(lib->settings,
                                     "%s.plugins.eap-simaka-leemobi.sim_url", NULL,
                                     lib->ns);

    DBG1(DBG_LIB, "eap_simaka_leemobi_card_create: %s", sim_url);

    INIT(this,
		.public = {
			.card = {
				.get_triplet = _get_triplet,
				.get_quintuplet = _get_quintuplet,
				.resync = _resync,
				.get_pseudonym = (void*)return_null,
				.set_pseudonym = (void*)nop,
				.get_reauth = (void*)return_null,
				.set_reauth = (void*)nop,
			},
			.destroy = _destroy,
		},
		.sim_url = sim_url,
	);

	return &this->public;
}
