/* compat.c
 * Copyright (c) 2012, Peter Ohler
 * All rights reserved.
 */

#include <stdio.h>

#include "oj.h"
#include "err.h"
#include "parse.h"
#include "resolve.h"
#include "hash.h"
#include "encode.h"

static void
hash_set_cstr(ParseInfo pi, Val kval, const char *str, size_t len, const char *orig) {
    const char		*key = kval->key;
    int			klen = kval->klen;
    Val			parent = stack_peek(&pi->stack);
    volatile VALUE	rkey = kval->key_val;

    if (Qundef == rkey &&
	Yes == pi->options.create_ok &&
	0 != pi->options.create_id &&
	*pi->options.create_id == *key &&
	(int)pi->options.create_id_len == klen &&
	0 == strncmp(pi->options.create_id, key, klen)) {
	parent->classname = oj_strndup(str, len);
	parent->clen = len;
    } else {
	volatile VALUE	rstr = rb_str_new(str, len);

	if (Qundef == rkey) {
	    rkey = rb_str_new(key, klen);
	    rstr = oj_encode(rstr);
	    rkey = oj_encode(rkey);
	    if (Yes == pi->options.sym_key) {
		rkey = rb_str_intern(rkey);
	    }
	}
	if (rb_cHash != rb_obj_class(parent->val)) {
	    // The rb_hash_set would still work but the unit tests for the
	    // json gem require the less efficient []= method be called to set
	    // values. Even using the store method to set the values will fail
	    // the unit tests.
	    rb_funcall(parent->val, rb_intern("[]="), 2, rkey, rstr);
	} else {
	    rb_hash_aset(parent->val, rkey, rstr);
	}
    }
}

static VALUE
start_hash(ParseInfo pi) {
    if (Qnil != pi->options.hash_class) {
	return rb_class_new_instance(0, NULL, pi->options.hash_class);
    }
    return rb_hash_new();
}

static void
end_hash(struct _ParseInfo *pi) {
    Val	parent = stack_peek(&pi->stack);

    if (0 != parent->classname) {
	VALUE	clas;

	clas = oj_name2class(pi, parent->classname, parent->clen, 0, rb_eArgError);
	if (Qundef != clas) { // else an error
	    parent->val = rb_funcall(clas, oj_json_create_id, 1, parent->val);
	}
	if (0 != parent->classname) {
	    xfree((char*)parent->classname);
	    parent->classname = 0;
	}
    }
}

static VALUE
calc_hash_key(ParseInfo pi, Val parent) {
    volatile VALUE	rkey = parent->key_val;

    if (Qundef == rkey) {
	rkey = rb_str_new(parent->key, parent->klen);
    }
    rkey = oj_encode(rkey);
    if (Yes == pi->options.sym_key) {
	rkey = rb_str_intern(rkey);
    }
    return rkey;
}

static void
add_num(ParseInfo pi, NumInfo ni) {
    pi->stack.head->val = oj_num_as_value(ni);
}

static void
hash_set_num(struct _ParseInfo *pi, Val parent, NumInfo ni) {
    if (rb_cHash != rb_obj_class(parent->val)) {
	// The rb_hash_set would still work but the unit tests for the
	// json gem require the less efficient []= method be called to set
	// values. Even using the store method to set the values will fail
	// the unit tests.
	rb_funcall(stack_peek(&pi->stack)->val, rb_intern("[]="), 2, calc_hash_key(pi, parent), oj_num_as_value(ni));
    } else {
	rb_hash_aset(stack_peek(&pi->stack)->val, calc_hash_key(pi, parent), oj_num_as_value(ni));
    }
}

static VALUE
start_array(ParseInfo pi) {
    if (Qnil != pi->options.array_class) {
	return rb_class_new_instance(0, NULL, pi->options.array_class);
    }
    return rb_ary_new();
}

static void
array_append_num(ParseInfo pi, NumInfo ni) {
    Val	parent = stack_peek(&pi->stack);
    
    if (rb_cArray != rb_obj_class(parent->val)) {
	// The rb_ary_push would still work but the unit tests for the json
	// gem require the less efficient << method be called to push the
	// values.
	rb_funcall(parent->val, rb_intern("<<"), 1, oj_num_as_value(ni));
    } else {
	rb_ary_push(parent->val, oj_num_as_value(ni));
    }
}

void
oj_set_compat_callbacks(ParseInfo pi) {
    oj_set_strict_callbacks(pi);
    pi->start_hash = start_hash;
    pi->end_hash = end_hash;
    pi->hash_set_cstr = hash_set_cstr;
    pi->add_num = add_num;
    pi->hash_set_num = hash_set_num;
    pi->start_array = start_array;
    pi->array_append_num = array_append_num;
}

VALUE
oj_compat_parse(int argc, VALUE *argv, VALUE self) {
    struct _ParseInfo	pi;

    parse_info_init(&pi);
    pi.options = oj_default_options;
    pi.handler = Qnil;
    pi.err_class = Qnil;
    pi.max_depth = 0;
    pi.options.allow_nan = Yes;
    pi.options.nilnil = Yes;
    oj_set_compat_callbacks(&pi);

    if (T_STRING == rb_type(*argv)) {
	return oj_pi_parse(argc, argv, &pi, 0, 0, false);
    } else {
	return oj_pi_sparse(argc, argv, &pi, 0);
    }
}

VALUE
oj_compat_parse_cstr(int argc, VALUE *argv, char *json, size_t len) {
    struct _ParseInfo	pi;

    parse_info_init(&pi);
    pi.options = oj_default_options;
    pi.handler = Qnil;
    pi.err_class = Qnil;
    pi.max_depth = 0;
    pi.options.allow_nan = Yes;
    pi.options.nilnil = Yes;
    oj_set_strict_callbacks(&pi);
    pi.end_hash = end_hash;
    pi.hash_set_cstr = hash_set_cstr;

    return oj_pi_parse(argc, argv, &pi, json, len, false);
}
