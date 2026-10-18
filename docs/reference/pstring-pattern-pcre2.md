# pcre2

- [`pstrexpr_pcre2_vt`](#pstrexpr_pcre2_vt) — The vtable, in case you want to register it under a different name.
- [`pstrexpr_register_pcre2`](#pstrexpr_register_pcre2) — Registers PCRE2 under `prefix`, "pcre" if it's `NULL`.

<a id="pstrexpr_pcre2_vt"></a>

## `pstrexpr_pcre2_vt`

```c
static const struct pstrexpr_vt pstrexpr_pcre2_vt;
```

The vtable, in case you want to register it under a different name.

<a id="pstrexpr_register_pcre2"></a>

## `pstrexpr_register_pcre2`

```c
PSTR_INLINE int pstrexpr_register_pcre2(const char *prefix);
```

Registers PCRE2 under `prefix`, "pcre" if it's `NULL`.

**Errors**

see `pstrexpr_register`.

