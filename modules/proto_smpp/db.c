/*
 * Copyright (C) 2019 - OpenSIPS Project
 *
 * This file is part of opensips, a free SIP server.
 *
 * opensips is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * opensips is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 *
 */

#include "../../str.h"
#include "../../resolve.h"
#include "proto_smpp.h"
#include "../../db/db.h"
#include "db.h"

static db_con_t* smpp_db_handle;
static db_func_t smpp_dbf;

str smpp_table = str_init("smpp"); /* Name of smpp table */
str smpp_ip_col = str_init("ip");       /* Name of ip address column */
str smpp_port_col = str_init("port"); /* Name of port column */
str smpp_system_id_col = str_init("system_id");
str smpp_password_col = str_init("password");
str smpp_system_type_col = str_init("system_type");
str smpp_src_ton_col = str_init("src_ton");
str smpp_src_npi_col = str_init("src_npi");
str smpp_dst_ton_col = str_init("dst_ton");
str smpp_dst_npi_col = str_init("dst_npi");
str smpp_session_type_col = str_init("session_type");

int smpp_db_bind(const str *db_url)
{
	if (db_bind_mod(db_url, &smpp_dbf)) {
		LM_ERR("cannot bind module database\n");
		return -1;
	}
	return 0;
}

int smpp_db_init(const str *db_url)
{
	if (smpp_dbf.init == 0) {
		LM_ERR("unbound database module\n");
		return -1;
	}
	smpp_db_handle = smpp_dbf.init(db_url);
	if (smpp_db_handle == 0){
		LM_ERR("cannot initialize database connection\n");
		return -1;
	}
	return 0;
}

int smpp_query(const str *smpp_table, db_key_t *cols, int col_nr, db_res_t **res)
{
	if (smpp_dbf.use_table(smpp_db_handle, smpp_table) < 0) {
		LM_ERR("error while trying to use smpp table\n");
		return -1;
	}

	if (smpp_dbf.query(smpp_db_handle, NULL, 0, NULL, cols, 0, col_nr, 0, res) < 0) {
		LM_ERR("error while querying database\n");
		return -1;
	}

	return 0;
}

void smpp_free_results(db_res_t *res)
{
	smpp_dbf.free_result(smpp_db_handle, res);
}

void smpp_db_close(void)
{
	if (smpp_db_handle && smpp_dbf.close) {
		smpp_dbf.close(smpp_db_handle);
		smpp_db_handle = 0;
	}
}

void build_smpp_sessions_from_db(void)
{
	struct ip_addr *ip;
	db_key_t cols[10];
	db_res_t* res = NULL;
	db_row_t* row;
	db_val_t* val;
	smpp_session_t *session;
	str ip_s, system_s, pass_s, type_s;

	int i;

	cols[0] = &smpp_ip_col;
	cols[1] = &smpp_port_col;
	cols[2] = &smpp_system_id_col;
	cols[3] = &smpp_password_col;
	cols[4] = &smpp_system_type_col;
	cols[5] = &smpp_src_ton_col;
	cols[6] = &smpp_src_npi_col;
	cols[7] = &smpp_dst_ton_col;
	cols[8] = &smpp_dst_npi_col;
	cols[9] = &smpp_session_type_col;

	if (smpp_query(&smpp_table, cols, 10, &res) < 0)
		return;

	row = RES_ROWS(res);

	LM_DBG("Number of rows in domain table: %d\n", RES_ROW_N(res));

	for (i = 0; i < RES_ROW_N(res); i++) {
		val = ROW_VALUES(row + i);
		if (VAL_TYPE(val) == DB_STRING) {
			ip_s.s = (char *)VAL_STRING(val);
			ip_s.len = strlen(ip_s.s);
		} else if (VAL_TYPE(val) == DB_STR) {
			ip_s = VAL_STR(val);
		} else {
			LM_ERR("invalid column type %d for ip (row %d)\n", VAL_TYPE(val), i);
			continue;
		}
		ip = str2ip(&ip_s);
		if (!ip) {
			LM_ERR("Invalid IP [%.*s] for row %d\n", ip_s.len, ip_s.s, i);
		}
		if (VAL_TYPE(val + 1) != DB_INT) {
			LM_ERR("invalid column type %d for port (row %d)\n", VAL_TYPE(val + 1), i);
			continue;
		}
		if (VAL_TYPE(val + 2) == DB_STRING) {
			system_s.s = (char *)VAL_STRING(val + 2);
			system_s.len = strlen(system_s.s);
		} else if (VAL_TYPE(val + 2) == DB_STR) {
			system_s = VAL_STR(val + 2);
		} else {
			LM_ERR("invalid column type %d for system id (row %d)\n", VAL_TYPE(val + 2), i);
			continue;
		}
		if (VAL_TYPE(val + 3) == DB_STRING) {
			pass_s.s = (char *)VAL_STRING(val + 3);
			pass_s.len = strlen(pass_s.s);
		} else if (VAL_TYPE(val + 3) == DB_STR) {
			pass_s = VAL_STR(val + 3);
		} else {
			LM_ERR("invalid column type %d for password (row %d)\n", VAL_TYPE(val + 3), i);
			continue;
		}
		if (VAL_TYPE(val + 4) == DB_STRING) {
			type_s.s = (char *)VAL_STRING(val + 4);
			type_s.len = strlen(type_s.s);
		} else if (VAL_TYPE(val + 4) == DB_STR) {
			type_s = VAL_STR(val + 4);
		} else {
			LM_ERR("invalid column type %d for password (row %d)\n", VAL_TYPE(val + 4), i);
			continue;
		}
		if (VAL_TYPE(val + 5) != DB_INT) {
			LM_ERR("invalid column type %d for src ton (row %d)\n", VAL_TYPE(val + 5), i);
			continue;
		}
		if (VAL_TYPE(val + 6) != DB_INT) {
			LM_ERR("invalid column type %d for src npi (row %d)\n", VAL_TYPE(val + 6), i);
			continue;
		}
		if (VAL_TYPE(val + 7) != DB_INT) {
			LM_ERR("invalid column type %d for dst ton (row %d)\n", VAL_TYPE(val + 7), i);
			continue;
		}
		if (VAL_TYPE(val + 8) != DB_INT) {
			LM_ERR("invalid column type %d for dst npi (row %d)\n", VAL_TYPE(val + 8), i);
			continue;
		}
		if (VAL_TYPE(val + 9) != DB_INT) {
			LM_ERR("invalid column type %d for session type (row %d)\n", VAL_TYPE(val + 9), i);
			continue;
		}
		session = smpp_session_new(ip, VAL_INT(val + 1), &system_s,
				&pass_s, &type_s, VAL_INT(val + 5), VAL_INT(val + 6),
				VAL_INT(val + 7), VAL_INT(val + 8), VAL_INT(val + 9));
		if (!session) {
			LM_ERR("cannot add session in row %d\n", i);
			continue;
		}
	}
	smpp_free_results(res);
}