/*
 * gsqlconnect_mysql.c
 *
 *      Copyright 2010 薇菜工作室
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */


/**
 * SECTION:gsqlconnect_mysql
 * @short_description: GSQLConnectMysql 实现 mysql 数据库连接
 * @title:数据库连接后端
 * @see_also: #GSQLConnect
 * @stability: Stable
 * @include: monitor/gsqlconnect_mysql.h
 *
 * #GSQLConnectMysql 是 #GSQLConnect 的一个实现，用来连接到 mysql 数据库
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib.h>
#include <glib-object.h>

#include <glib/gi18n.h>
#include "global.h"
#include "gsqlconnect.h"
#include "gsqlconnect_mysql.h"
#include "mysql_static_template.h"

enum G_SQL_CONNECT_MYSQL_PROPERTY{
	GSQL_CONNECT_MYSQL_HOST = 1,
	GSQL_CONNECT_MYSQL_USER,
	GSQL_CONNECT_MYSQL_PASSWD,
	GSQL_CONNECT_MYSQL_DB
};

static void g_sql_connect_mysql_class_init(GSQLConnectMysqlClass * klass);
static void g_sql_connect_mysql_init(GSQLConnectMysql * obj);
static void g_sql_connect_mysql_finalize(GObject * obj);
static void g_sql_connect_mysql_set_property(GObject *object,guint property_id, const GValue *value, GParamSpec *pspec);
static void g_sql_connect_mysql_get_property(GObject *object,guint property_id, GValue *value, GParamSpec *pspec);

static gboolean g_sql_connect_mysql_real_connect(GSQLConnect * obj,GError ** err);
static gboolean g_sql_connect_mysql_ping(GSQLConnect * obj,GError ** );
static void		g_sql_connect_mysql_create_db(GSQLConnectMysql*mobj,const char * db);
static gboolean g_sql_connect_mysql_check_config(GSQLConnect * obj);
static void g_sql_connect_mysql_register_property(GSQLConnectMysqlClass * klass);
static gboolean	g_sql_connect_mysql_real_query(GSQLConnect*,const char * sql_stmt,gsize len /* -1 for nul-terminated string*/);
static gboolean	g_sql_connect_mysql_get_row(GSQLResult * obj);
static void g_sql_connect_mysql_free_result(GSQLResult * );

G_DEFINE_TYPE(GSQLConnectMysql,g_sql_connect_mysql,G_TYPE_SQL_CONNNECT);


void g_sql_connect_mysql_class_init(GSQLConnectMysqlClass * klass)
{
	GObjectClass	* gobjclass = G_OBJECT_CLASS(klass);
	GSQLConnectClass * parent_class = (GSQLConnectClass*)klass;
	parent_class->connect = g_sql_connect_mysql_real_connect;
	parent_class->check_config = g_sql_connect_mysql_check_config;
	gobjclass->set_property = g_sql_connect_mysql_set_property;
	gobjclass->get_property = g_sql_connect_mysql_get_property;
	parent_class->run_query = g_sql_connect_mysql_real_query;
	gobjclass->finalize = g_sql_connect_mysql_finalize;
	parent_class->ping = g_sql_connect_mysql_ping;

	g_sql_connect_mysql_register_property(klass);

	g_sql_connect_thread_init = (typeof(g_sql_connect_thread_init))mysql_thread_init;
	g_sql_connect_thread_end  = mysql_thread_end;
}

void g_sql_connect_mysql_init(GSQLConnectMysql * obj)
{
	gboolean	bltrue=TRUE;

	mysql_init(obj->mysql);
	mysql_options(obj->mysql,MYSQL_OPT_RECONNECT,&bltrue);

}

void g_sql_connect_mysql_finalize(GObject * obj)
{
	GSQLConnectMysql * mobj = (GSQLConnectMysql*)obj;

	mysql_close(mobj->mysql);
}


gboolean g_sql_connect_mysql_real_connect(GSQLConnect * obj,GError ** err)
{
	g_assert(IS_G_SQL_CONNECT_MYSQL(obj));

	GSQLConnectMysql * mobj = (GSQLConnectMysql*)obj;

	gchar * host,*user,*passwd,*db;
	g_object_get(obj,"host",&host,"user",&user,"passwd",&passwd,"dbname",&db,NULL);

	if (mysql_real_connect(mobj->mysql, host, user, passwd, db, 0, NULL, 0))
	{
		mysql_set_character_set(mobj->mysql,"utf8");
		return TRUE;
	}else
	{
		if(mysql_real_connect(mobj->mysql, host, user, passwd, NULL, 0, NULL, 0))
		{
			mysql_set_character_set(mobj->mysql,"utf8");
			g_sql_connect_mysql_create_db(mobj,db);
			return TRUE;
		}
	}
	g_set_error_literal(err,g_quark_from_static_string(GETTEXT_PACKAGE),mysql_errno(mobj->mysql),mysql_error(mobj->mysql));
	return FALSE;
}

gboolean g_sql_connect_mysql_ping(GSQLConnect * obj,GError ** err)
{
	g_assert(IS_G_SQL_CONNECT_MYSQL(obj));
	GSQLConnectMysql * mobj = (GSQLConnectMysql*)obj;

	return !mysql_ping(mobj->mysql);
}

gboolean	g_sql_connect_mysql_real_query(GSQLConnect*obj,const char * sql_stmt,gsize len)
{
	MYSQL_FIELD * fields ;
	MYSQL_RES * myresult ;

	g_assert(IS_G_SQL_CONNECT_MYSQL(obj));
	GSQLConnectMysql * mobj = (GSQLConnectMysql*)obj;

	if (mysql_query(mobj->mysql, sql_stmt))
	{
		g_signal_emit_by_name(obj, "query-err",mysql_errno(mobj->mysql), mysql_error(mobj->mysql));
		return FALSE;
	}

	myresult = mysql_store_result(mobj->mysql);

	if (!myresult )
	{
		obj->lastresult = NULL;
		return TRUE;
	}

/*------------------------------------------
	建立 GSQLResult 对象，并存储返回的数据,设置 GSQLresult 虚表使用 mysql 特定方法 :)
	*/
	GSQLResult * result =  g_object_new(G_TYPE_SQL_RESULT,
			"type",G_TYPE_SQL_CONNNECT_MYSQL,"result",myresult,
			"fields",mysql_num_fields(myresult),NULL);

	obj->lastresult = (GObject*)result;

	result->nextrow = g_sql_connect_mysql_get_row;

	while (fields = mysql_fetch_field(myresult))
	{
		g_sql_result_append_result_array(result,fields->name);
	}

	result->freerows = g_sql_connect_mysql_free_result ;

	return TRUE;
}

gboolean	g_sql_connect_mysql_get_row(GSQLResult * obj)
{
	MYSQL_RES * myres = (MYSQL_RES*)obj->result;

	GStrv row = mysql_fetch_row(myres);

	obj->currow = row;

	return (row!=NULL);
}

gboolean	g_sql_connect_mysql_seek_row(GSQLResult * obj,guint offset)
{
	MYSQL_RES * myres = (MYSQL_RES*)obj->result;

	mysql_data_seek(myres,offset);

	GStrv row = mysql_fetch_row(myres);

	obj->currow = row;

	return (row!=NULL);
}

void g_sql_connect_mysql_free_result(GSQLResult * obj)
{
	mysql_free_result((MYSQL_RES*)(obj->result));
}


void g_sql_connect_mysql_create_db(GSQLConnectMysql*mobj,const char * db)
{
	gchar *sql =  g_strdup_printf("create database %s",db);
	mysql_query(mobj->mysql,sql);
	g_free(sql);

	mysql_select_db(mobj->mysql,db);

	for (int i = 0; i < (int) (sizeof(create_sql) / sizeof(char*)); ++i)
		mysql_query(mobj->mysql, create_sql[i]);
}

#define INSTALL_PROPERTY_STRING(klass,id,name,defaultval,type) \
		g_object_class_install_property(klass,id, \
			g_param_spec_string(name,name,name,defaultval,type))

void g_sql_connect_mysql_register_property(GSQLConnectMysqlClass * klass)
{
	GObjectClass* objclass =(GObjectClass*) klass ;
	INSTALL_PROPERTY_STRING(objclass,GSQL_CONNECT_MYSQL_HOST,"host",NULL,G_PARAM_CONSTRUCT|G_PARAM_READWRITE);
	INSTALL_PROPERTY_STRING(objclass,GSQL_CONNECT_MYSQL_USER,"user","root",G_PARAM_CONSTRUCT|G_PARAM_READWRITE);
	INSTALL_PROPERTY_STRING(objclass,GSQL_CONNECT_MYSQL_PASSWD,"passwd",NULL,G_PARAM_CONSTRUCT|G_PARAM_READWRITE);
	INSTALL_PROPERTY_STRING(objclass,GSQL_CONNECT_MYSQL_DB,"dbname","hotel",G_PARAM_CONSTRUCT|G_PARAM_READWRITE);
}

void g_sql_connect_mysql_set_property(GObject *object, guint property_id,
		const GValue *value, GParamSpec *pspec)
{
	GSQLConnectMysql * mobj = (GSQLConnectMysql*) object;

	g_return_if_fail(IS_G_SQL_CONNECT_MYSQL(mobj));

	switch (property_id)
	{
	case GSQL_CONNECT_MYSQL_HOST ... GSQL_CONNECT_MYSQL_DB:
		g_free(mobj->porperty_string[property_id]);
		mobj->porperty_string[property_id] = g_value_dup_string(value);
		break;
	default:
		g_warn_if_reached();
		break;
	}
}

void g_sql_connect_mysql_get_property(GObject *object,
		guint property_id, GValue *value, GParamSpec *pspec)
{
	GSQLConnectMysql * mobj = (GSQLConnectMysql*)object;

	g_return_if_fail(IS_G_SQL_CONNECT_MYSQL(mobj));

	switch (property_id)
	{
	case GSQL_CONNECT_MYSQL_HOST ... GSQL_CONNECT_MYSQL_DB:
		g_value_set_static_string(value, mobj->porperty_string[property_id]);
		break;
	default:
		g_warn_if_reached();
		break;
	}
}

gboolean g_sql_connect_mysql_check_config(GSQLConnect * obj)
{
	g_assert(IS_G_SQL_CONNECT_MYSQL(obj));

	gchar * g_db = g_key_file_get_string(gkeyfile,"mysql","db",NULL);
	gchar * g_user = g_key_file_get_string(gkeyfile,"mysql","user",NULL);
	gchar * g_passwd = g_key_file_get_string(gkeyfile,"mysql","passwd",NULL);

	if (g_db)
	{
		g_strchomp(g_strchug(g_user));
		if (strlen(g_db))
		{
			g_object_set(obj,"dbname",g_db,NULL);
		}
	}

	if(g_user)
	{
		g_strchomp(g_strchug(g_user));
		g_object_set(obj,"user",g_user,NULL);
	}
	if(g_passwd)
	{
		g_strchomp(g_strchug(g_passwd));
		g_object_set(obj,"passwd",g_passwd,NULL);
	}
	g_free(g_passwd);
	g_free(g_user);
	g_free(g_db);
	return TRUE;
}
