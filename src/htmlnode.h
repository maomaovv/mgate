/*
 * htmlnote.h
 *
 *  Created on: 2010-4-25
 *      Author: cai
 */




#ifndef HTMLNOTE_H_
#define HTMLNOTE_H_

#include <glib.h>

/**
 * HtmlNode:
 * @parent: 父节点
 * @children: 子节点链表
 * @tag: html 标记， 不带  < > 比如 <table> </table> 标记对在 tag 上保存为 table
 * @plane: 如果不是 html 标记，而是平坦文字，则此处为文字指针
 * @attr: html 标记的各种熟悉，比如 <table align="center" /> 则 attr 就是 align="center"
 * 		  一个属性是一个字符串，@attr 是个字符串链表
 *
 * html 页面的标记被由 一个 HtmlNode 表示
 */
typedef struct _HtmlNode{
	struct _HtmlNode * parent; //parent
	GList * children; //children
    const char		*	tag; // < > </> the tag
    const char		* 	plane; // not a tag ,but plane text
    GList* attr; // attrs
}HtmlNode;

/**
 * htmlnode_appender:
 * @thunk: 一小段字符串
 * @data: 用户参数
 *
 * #htmlnode_appender 用来提供回调，htmlnode_to_plane_text() 每次解析出一块 html 文字，就会
 * 调用 #htmlnode_appender 来让用户自己处理。全部 #htmlnode_appender 的回调合起来就组成了一个
 * 完整的由  @rootnode 表述的 html 页面
 */
typedef void (*htmlnode_appender)(const char * thunk, gpointer data);

G_BEGIN_DECLS

HtmlNode * htmlnode_append_child(HtmlNode * node,HtmlNode * child);
HtmlNode * htmlnode_append_attr(HtmlNode * node, const char * attr);
HtmlNode * htmlnode_append_attr_take(HtmlNode * node, char * attr);

HtmlNode * htmlnode_new(HtmlNode * parent, const char * tag , ...);
HtmlNode * htmlnode_new_text(HtmlNode * parent, const char * text );
HtmlNode * htmlnode_newv(HtmlNode * parent, const char * tag, va_list v);
HtmlNode * htmlnode_new_head(HtmlNode * parent, const char * attrlist, ...);
HtmlNode * htmlnode_new_body(HtmlNode * parent, const char * attrlist, ...);
HtmlNode * htmlnode_new_table(HtmlNode * parent,const char * attrlist, ...);
HtmlNode * htmlnode_new_form(HtmlNode * parent,const char * method , const char * action ,const char * attrlist, ...);
HtmlNode * htmlnode_new_iframe(HtmlNode * parent,const char * src ,const char * attrlist, ...);

//释放一棵树
void htmlnode_free(HtmlNode * rootnode);

gboolean htmlnode_to_plane_text(HtmlNode * rootnode, htmlnode_appender , gpointer user_data );

//同时 free
gboolean htmlnode_to_plane_text_and_free(HtmlNode * rootnode, htmlnode_appender appender , gpointer user_data );

void htmlnode_free(HtmlNode * rootnode);
G_END_DECLS

#endif /* HTMLNOTE_H_ */
