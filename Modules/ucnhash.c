/* unicode character name tables */
/* rewritten for Python 2.1 by Fredrik Lundh (fredrik@pythonware.com) */

#include "Python.h"
#include "ucnhash.h"

/* data file generated by Tools/unicode/makeunicodedata.py */
#include "unicodename_db.h"

/* -------------------------------------------------------------------- */
/* database code (cut and pasted from the unidb package) */

static unsigned long
gethash(const char *s, int len)
{
    int i;
    unsigned long h = 0;
    unsigned long ix;
    for (i = 0; i < len; i++) {
        /* magic value 47 was chosen to minimize the number
           of collisions for the uninames dataset.  see the
           makeunicodedata script for more background */
        h = (h * 47) + (unsigned char) toupper(s[i]);
        ix = h & 0xff000000;
        if (ix)
            h = (h ^ ((ix>>24) & 0xff)) & 0x00ffffff;
    }
    return h;
}

static int
getname(Py_UCS4 code, char* buffer, int buflen)
{
    int offset;
    int i;
    int word;
    unsigned char* w;

    if (code < 0 || code >= 65536)
        return 0;

    /* get offset into phrasebook */
    offset = phrasebook_offset1[(code>>SHIFT)];
    offset = phrasebook_offset2[(offset<<SHIFT)+(code&((1<<SHIFT)-1))];
    if (!offset)
        return 0;

    i = 0;

    for (;;) {
        /* get word index */
        if (phrasebook[offset] & 128) {
            word = phrasebook[offset] & 127;
            offset++;
        } else {
            word = (phrasebook[offset]<<8) + phrasebook[offset+1];
            offset+=2;
        }
        if (i) {
            if (i > buflen)
                return 0; /* buffer overflow */
            buffer[i++] = ' ';
        }
        /* copy word string from lexicon.  the last character in the
           word has bit 7 set.  the last word in a string ends with
           0x80 */
        w = lexicon + lexicon_offset[word];
        while (*w < 128) {
            if (i >= buflen)
                return 0; /* buffer overflow */
            buffer[i++] = *w++;
        }
        if (i >= buflen)
            return 0; /* buffer overflow */
        buffer[i++] = *w & 127;
        if (*w == 128)
            break; /* end of word */
    }

    return 1;
}

static int
cmpname(int code, const char* name, int namelen)
{
    /* check if code corresponds to the given name */
    int i;
    char buffer[NAME_MAXLEN];
    if (!getname(code, buffer, sizeof(buffer)))
        return 0;
    for (i = 0; i < namelen; i++) {
        if (toupper(name[i]) != buffer[i])
            return 0;
    }
    return buffer[namelen] == '\0';
}

static int
getcode(const char* name, int namelen, Py_UCS4* code)
{
    unsigned int h, v;
    unsigned int mask = CODE_SIZE-1;
    unsigned int i, incr;

    /* the following is the same as python's dictionary lookup, with
       only minor changes.  see the makeunicodedata script for more
       details */

    h = (unsigned int) gethash(name, namelen);
    i = (~h) & mask;
    v = code_hash[i];
    if (!v)
        return 0;
    if (cmpname(v, name, namelen)) {
        *code = v;
        return 1;
    }
    incr = (h ^ (h >> 3)) & mask;
    if (!incr)
        incr = mask;
    for (;;) {
        i = (i + incr) & mask;
        v = code_hash[i];
        if (!v)
            return -1;
        if (cmpname(v, name, namelen)) {
            *code = v;
            return 1;
        }
        incr = incr << 1;
        if (incr > mask)
            incr = incr ^ CODE_POLY;
    }
}

static const _PyUnicode_Name_CAPI hashAPI = 
{
    sizeof(_PyUnicode_Name_CAPI),
    getname,
    getcode
};

/* -------------------------------------------------------------------- */
/* Python bindings */

static PyObject *
ucnhash_getname(PyObject* self, PyObject* args)
{
    char name[NAME_MAXLEN];

    int code;
    if (!PyArg_ParseTuple(args, "i", &code))
        return NULL;

    if (!getname((Py_UCS4) code, name, sizeof(name))) {
        PyErr_SetString(PyExc_ValueError, "undefined character code");
        return NULL;
    }

    return Py_BuildValue("s", name);
}

static PyObject *
ucnhash_getcode(PyObject* self, PyObject* args)
{
    Py_UCS4 code;

    char* name;
    int namelen;
    if (!PyArg_ParseTuple(args, "s#", &name, &namelen))
        return NULL;

    if (!getcode(name, namelen, &code)) {
        PyErr_SetString(PyExc_ValueError, "undefined character name");
        return NULL;
    }

    return Py_BuildValue("i", code);
}

static  
PyMethodDef ucnhash_methods[] =
{   
    {"getname", ucnhash_getname, 1},
    {"getcode", ucnhash_getcode, 1},
    {NULL, NULL},
};

static char *ucnhash_docstring = "ucnhash hash function module";


/* Create PyMethodObjects and register them in the module's dict */
DL_EXPORT(void) 
initucnhash(void)
{
    PyObject *m, *d, *v;

    m = Py_InitModule4(
        "ucnhash", /* Module name */
        ucnhash_methods, /* Method list */
        ucnhash_docstring, /* Module doc-string */
        (PyObject *)NULL, /* always pass this as *self */
        PYTHON_API_VERSION); /* API Version */
    if (!m)
        return;

    d = PyModule_GetDict(m);
    if (!d)
        return;

    /* Export C API */
    v = PyCObject_FromVoidPtr((void *) &hashAPI, NULL);
    PyDict_SetItemString(d, "Unicode_Names_CAPI", v);
    Py_XDECREF(v);
}
