# onig

- [`pstrexpr_onig_vt`](#pstrexpr_onig_vt) — The vtable, in case you want to register it under a different name.
- [`pstrexpr_register_onig`](#pstrexpr_register_onig) — Initializes Oniguruma for UTF-8 and registers it under `prefix`, "onig" if it's `NULL`.

<a id="pstrexpr_onig_vt"></a>

## `pstrexpr_onig_vt`

```c
static const struct pstrexpr_vt pstrexpr_onig_vt;
```

The vtable, in case you want to register it under a different name.

<a id="pstrexpr_register_onig"></a>

## `pstrexpr_register_onig`

```c
PSTR_INLINE int pstrexpr_register_onig(const char *prefix);
```

Initializes Oniguruma for UTF-8 and registers it under `prefix`,
"onig" if it's `NULL`. Errors: see `pstrexpr_register`.

> libonig exports its own POSIX-style `regcomp` and `regexec`. In a program
> that links it, these replace the C library's, which silently breaks
> `<pstring/pattern/posix.h>` (it would run Oniguruma's BRE/ERE emulation).
> Don't use the two adapters in the same program.

