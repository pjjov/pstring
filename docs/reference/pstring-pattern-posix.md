# posix

- [`pstrexpr_posix_vt`](#pstrexpr_posix_vt) — The vtable, in case you want to register it under a different name.
- [`pstrexpr_register_posix`](#pstrexpr_register_posix) — Registers POSIX regex under `prefix`, "posix" if it's `NULL`.

<a id="pstrexpr_posix_vt"></a>

## `pstrexpr_posix_vt`

```c
static const struct pstrexpr_vt pstrexpr_posix_vt;
```

The vtable, in case you want to register it under a different name.

<a id="pstrexpr_register_posix"></a>

## `pstrexpr_register_posix`

```c
PSTR_INLINE int pstrexpr_register_posix(const char *prefix);
```

Registers POSIX regex under `prefix`, "posix" if it's `NULL`.

**Errors**

see `pstrexpr_register`.

