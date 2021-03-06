/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file kvline.c
 *
 * \brief Manipulating lines of key-value pairs.
 **/

#include "orconfig.h"

#include "lib/container/smartlist.h"
#include "lib/encoding/confline.h"
#include "lib/encoding/cstring.h"
#include "lib/encoding/kvline.h"
#include "lib/malloc/malloc.h"
#include "lib/string/compat_ctype.h"
#include "lib/string/printf.h"
#include "lib/string/util_string.h"
#include "lib/log/escape.h"
#include "lib/log/util_bug.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/** Return true iff we need to quote and escape the string <b>s</b> to encode
 * it. */
static bool
needs_escape(const char *s, bool as_keyless_val)
{
  if (as_keyless_val && *s == 0)
    return true;

  for (; *s; ++s) {
    if (*s >= 127 || TOR_ISSPACE(*s) || ! TOR_ISPRINT(*s) ||
        *s == '\'' || *s == '\"') {
      return true;
    }
  }
  return false;
}

/**
 * Return true iff the key in <b>line</b> is not set.
 **/
static bool
line_has_no_key(const config_line_t *line)
{
  return line->key == NULL || strlen(line->key) == 0;
}

/**
 * Return true iff the all the lines in <b>line</b> can be encoded
 * using <b>flags</b>.
 **/
static bool
kvline_can_encode_lines(const config_line_t *line, unsigned flags)
{
  for ( ; line; line = line->next) {
    const bool keyless = line_has_no_key(line);
    if (keyless) {
      if (! (flags & KV_OMIT_KEYS)) {
        /* If KV_OMIT_KEYS is not set, we can't encode a line with no key. */
        return false;
      }
      if (strchr(line->value, '=') && !( flags & KV_QUOTED)) {
        /* We can't have a keyless value with = without quoting it. */
        return false;
      }
    }

    if (needs_escape(line->value, keyless) && ! (flags & KV_QUOTED)) {
      /* If KV_QUOTED is false, we can't encode a value that needs quotes. */
      return false;
    }
    if (line->key && strlen(line->key) &&
        (needs_escape(line->key, false) || strchr(line->key, '='))) {
      /* We can't handle keys that need quoting. */
      return false;
    }
  }
  return true;
}

/**
 * Encode a linked list of lines in <b>line</b> as a series of 'Key=Value'
 * pairs, using the provided <b>flags</b> to encode it.  Return a newly
 * allocated string on success, or NULL on failure.
 *
 * If KV_QUOTED is set in <b>flags</b>, then all values that contain
 * spaces or unusual characters are escaped and quoted.  Otherwise, such
 * values are not allowed.
 *
 * If KV_OMIT_KEYS is set in <b>flags</b>, then pairs with empty keys are
 * allowed, and are encoded as 'Value'.  Otherwise, such pairs are not
 * allowed.
 */
char *
kvline_encode(const config_line_t *line,
              unsigned flags)
{
  if (!kvline_can_encode_lines(line, flags))
    return NULL;

  smartlist_t *elements = smartlist_new();

  for (; line; line = line->next) {

    const char *k = "";
    const char *eq = "=";
    const char *v = "";
    const bool keyless = line_has_no_key(line);
    bool esc = needs_escape(line->value, keyless);
    char *tmp = NULL;

    if (! keyless) {
      k = line->key;
    } else {
      eq = "";
      if (strchr(line->value, '=')) {
        esc = true;
      }
    }

    if (esc) {
      tmp = esc_for_log(line->value);
      v = tmp;
    } else {
      v = line->value;
    }

    smartlist_add_asprintf(elements, "%s%s%s", k, eq, v);
    tor_free(tmp);
  }

  char *result = smartlist_join_strings(elements, " ", 0, NULL);

  SMARTLIST_FOREACH(elements, char *, cp, tor_free(cp));
  smartlist_free(elements);

  return result;
}

/**
 * Decode a <b>line</b> containing a series of space-separated 'Key=Value'
 * pairs, using the provided <b>flags</b> to decode it.  Return a newly
 * allocated list of pairs on success, or NULL on failure.
 *
 * If KV_QUOTED is set in <b>flags</b>, then (double-)quoted values are
 * allowed. Otherwise, such values are not allowed.
 *
 * If KV_OMIT_KEYS is set in <b>flags</b>, then values without keys are
 * allowed.  Otherwise, such values are not allowed.
 */
config_line_t *
kvline_parse(const char *line, unsigned flags)
{
  const char *cp = line, *cplast = NULL;
  bool omit_keys = (flags & KV_OMIT_KEYS) != 0;
  bool quoted = (flags & KV_QUOTED) != 0;

  config_line_t *result = NULL;
  config_line_t **next_line = &result;

  char *key = NULL;
  char *val = NULL;

  while (*cp) {
    key = val = NULL;
    {
      size_t vgc = strspn(cp, " \t\r\v\n");
      cp += vgc;
    }
    if (BUG(cp == cplast)) {
      /* If we didn't parse anything, this code is broken. */
      goto err; // LCOV_EXCL_LINE
    }
    cplast = cp;
    if (! *cp)
      break; /* End of string; we're done. */

    /* Possible formats are K=V, K="V", V, and "V", depending on flags. */

    /* Find the key. */
    if (*cp != '\"') {
      size_t vgc = strcspn(cp, " \t\r\v\n=");

      if (cp[vgc] == '=') {
        key = tor_memdup_nulterm(cp, vgc);
        cp += vgc + 1;
      } else {
        if (!omit_keys)
          goto err;
      }
    }

    if (*cp == '\"') {
      /* The type is "V". */
      if (!quoted)
        goto err;
      size_t len=0;
      cp = unescape_string(cp, &val, &len);
      if (cp == NULL || len != strlen(val)) {
        // The string contains a NUL or is badly coded.
        goto err;
      }
    } else {
      size_t vgc = strcspn(cp, " \t\r\v\n");
      val = tor_memdup_nulterm(cp, vgc);
      cp += vgc;
    }

    if (key && strlen(key) == 0) {
      /* We don't allow empty keys. */
      goto err;
    }

    *next_line = tor_malloc_zero(sizeof(config_line_t));
    (*next_line)->key = key ? key : tor_strdup("");
    (*next_line)->value = val;
    next_line = &(*next_line)->next;
    key = val = NULL;
  }

  if (!kvline_can_encode_lines(result, flags)) {
    goto err;
  }
  return result;

 err:
  tor_free(key);
  tor_free(val);
  config_free_lines(result);
  return NULL;
}
