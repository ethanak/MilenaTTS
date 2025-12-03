#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <sys/stat.h>
#include <milena.h>
#include <milena_mbrola.h>

#if PY_MAJOR_VERSION >= 3

#define PyInt_Check PyLong_Check
#define PyInt_AsLong PyLong_AsLong
#define PyString_Check PyUnicode_Check
#define PyString_AsString PyUnicode_AsUTF8
#define PyString_FromString PyUnicode_FromString
#define PyString_FromFormat PyUnicode_FromFormat


#define MOD_INIT(name) PyMODINIT_FUNC PyInit_##name(void)
#define MOD_RETVAL NULL
#define MOD_DEF(ob, name, doc, methods) \
        static struct PyModuleDef moduledef = { \
            PyModuleDef_HEAD_INIT, name, doc, -1, methods, }; \
        ob = PyModule_Create(&moduledef);

#define STRING8 "y"
#else
#define MOD_INIT(name) PyMODINIT_FUNC init##name(void)
#define MOD_RETVAL
#define MOD_DEF(ob, name, doc, methods) \
        ob = Py_InitModule3(name, methods, doc);

#define STRING8 "s"
#endif


static struct {
    char *name;
    int value;
} milena_consts[]={
    {"FLAG_IGNORE_TILDE",1},
    {"FLAG_ACCEPT_INFO",2},
    {"FLAG_IGNORE_HOURS",4},
    {"FLAG_SIMPLE_MODE",8},
    {"BREATH_NONE",0},
    {"BREATH_NORMAL",1},
    {"BREATH_DIALOG",2},
    {"BREATH_PREDIAL",3},
    {"BREATH_POSTDIAL",4},
    {"BREATH_LONG",5},
    {"PARTYPE_EMPTY", 0},
    {"PARTYPE_NORMAL", 1},
    {"PARTYPE_DIALOG",2},
    {"PHRASE_TYPE_MASK",7},
    {"PHRASE_TYPE_DOT",0},
    {"PHRASE_TYPE_COMMA",1},
    {"PHRASE_TYPE_QUESTION",2},
    {"PHRASE_TYPE_EXCLAMATION",3},
    {"PHRASE_TYPE_ELLIPSIS",4},
    {"PHRASE_TYPE_COLON",5},
    {"PHRASE_TYPE_BROKEN_ELLIPSIS",6},
    {"PHRASE_TYPE_FINAL_ELLIPSIS",7},
    {"PHRASE_DIALOG_MASK",8},
    {"PHRASE_FINAL_MASK",16},
    {"PHRASE_REVERSE_MASK",128},
    {NULL,0}};


typedef struct {
        PyObject_HEAD;
        struct milena *milena;
        struct milena_mbrola_cfg *mimbrola;
        char *ivoice;
        char *buf1,*buf2,*buf3,*ivo_buf;
        int blen1,blen2,blen3,ivo_len;
        int bookmode;
        int milena_flags;
        int rhvoice;
        struct phone_buffer phb;
} mil_MilenaSimObject;


static        char *error_str;
static        char *error_file;
static        int error_line;


static void freeme(mil_MilenaSimObject *self)
{
        free(self->buf1);
        free(self->buf2);
        free(self->buf3);
        if (self->ivo_buf) free(self->ivo_buf);
        if (self->ivoice) free(self->ivoice);
        if (self->milena) milena_Close(self->milena);
        if (self->mimbrola) milena_CloseModMbrola(self->mimbrola);
}

static int isfile(char *path)
{
        struct stat sb;
        return !stat(path,&sb);
}

static void clear_error(void)
{
    error_str=NULL;
    error_file=NULL;
    error_line=0;
}
static void errorfun(char *str,char *fname,int line)
{
    error_str=str;
    error_file=fname;
    error_line=line;
}

static void get_error(const char *str)
{
    if (!error_str) {
        PyErr_SetString( PyExc_RuntimeError,str);
    }
    else {
        if (error_file) {
            PyErr_Format(PyExc_RuntimeError,"%s w linii %d w pliku %s\n%s",str,error_line,error_file,error_str);
        }
        else if (error_line) {
            PyErr_Format(PyExc_RuntimeError,"%s w linii %d\n%s",str,error_line,error_str);
        }
        else {
            PyErr_Format(PyExc_RuntimeError,"%s\n%s",str,error_str);
        }
    }
    clear_error();
}

static int MilenaSim_init(mil_MilenaSimObject *self,
                        PyObject * args, PyObject *kwds)
{
    char *ivoice=NULL;
    int flags=-1;
    int rhvoice=0;
    static char *kwlist[]={"flags","ivoice","rhvoice",NULL};
    int bookmode=1;
    int morfologik=1;
    void *morfol_data=NULL;
    int morfol_loaded=0;
    
    if (! PyArg_ParseTupleAndKeywords(args, kwds, "|isi", kwlist,
                &flags,
                &ivoice,
                &rhvoice)) return -1;
    if ((flags & 8) && flags != -1) bookmode=morfologik=0;
    if (flags == -1) {
        flags=MILENA_PHR_IGNORE_INFO;
    }
    else {
        
        flags ^= MILENA_PHR_IGNORE_INFO;
    }
    self->buf1=malloc(self->blen1=8192);
    self->buf2=malloc(self->blen2=8192);
    self->buf3=malloc(self->blen3=8192);
    self->ivoice=ivoice?strdup(ivoice):NULL;
    if (rhvoice && !self->ivoice) self->ivoice=strdup("Ewa");
    self->rhvoice=rhvoice;
    if (ivoice) {
        self->ivo_buf=malloc(self->ivo_len=8192);
    }
    else {
        self->ivo_buf=NULL;
    }
    self->phb.str=NULL;
    self->phb.str_len=0;
    self->phb.buf_len=0;
    self->mimbrola=NULL;
    self->milena=NULL;
#ifndef MILENA_IVO_HAS_SUBPHR
    if (ivoice) bookmode=0;
#endif
    self->bookmode=bookmode;
    self->milena_flags=flags;
    
    self->milena=milena_Init(
        milena_FilePath((ivoice?"ive_pho.dat":"pl_pho.dat"),self->buf1),
        milena_FilePath("pl_dict.dat",self->buf2),
        milena_FilePath("pl_stress.dat",self->buf3));
    if (!self->milena) {
berror: freeme(self);
        get_error("Mileny nie można zainicjalizować");
        return -1;
    }
    milena_registerErrFun(self->milena,errorfun);
    if (!milena_ReadPhraser(self->milena,
                        milena_FilePath("pl_phraser.dat",self->buf1))) goto berror;
    if (!ivoice) {
        self->mimbrola=milena_InitModMbrola(
                                milena_FilePath("pl_mbrola.dat",self->buf1));
        if (!self->mimbrola) exit(1);
        if (!milena_ModMbrolaExtras(self->mimbrola,
                        milena_FilePath("pl_pro_mbrola.dat",self->buf1))) goto berror;
        if (!milena_ReadInton(self->mimbrola,
                        milena_FilePath("pl_intona.dat",self->buf1))) goto berror;
        if (bookmode) {
            milena_ModMbrolaSetFlag(
                                self->mimbrola,-1,MILENA_MBFL_BOOKMODE);
        }
    }
#ifdef MILENA_HAS_MORFOLOGIK
    if (morfologik) {
        morfol_data=milena_StartMorfologik(NULL);
        if (morfol_data) {
            morfol_loaded=milena_SetMorfologik(self->milena,morfol_data);
        }
    }
#endif

#if 0
    // to nieaktualne
    if (bookmode) {
        if (!morfol_loaded) {
            milena_ReadVerbs(self->milena,
                                milena_FilePath("pl_verbs.dat",self->buf1));
        }
    }
#endif
    if (!milena_ReadUserDic(self->milena,
                        milena_FilePath("pl_udict.dat",self->buf1))) goto berror;
    if (ivoice) {
        char *c;
        milena_ReadUserDic(self->milena,
                        milena_FilePath("ive_udict.dat",self->buf1));
        milena_ReadIvonaFin(self->milena,
                        milena_FilePath("ive_fin.dat",self->buf1));
        sprintf(self->buf2,"iv_%s_udic.dat",ivoice);
        for (c=self->buf2;*c;c++) *c=tolower(*c);
        milena_FilePath(self->buf2,self->buf1);
        if (isfile(self->buf1)) milena_ReadUserDic(self->milena,self->buf1);
        sprintf(self->buf2,"iv_%s_fin.dat",ivoice);
        for (c=self->buf2;*c;c++) *c=tolower(*c);
        milena_FilePath(self->buf2,self->buf1);
        if (isfile(self->buf1)) milena_ReadIvonaFin(self->milena,self->buf1);
    }
    if (self->milena_flags & 3) milena_SetPhraserMode(self->milena,self->milena_flags & 3);
    if (!(self->milena_flags & 4)  && !milena_ReadPhraser(self->milena,
                                milena_FilePath("pl_hours.dat",self->buf1))) goto berror;

    return 0; 
}

#if PY_MAJOR_VERSION < 3
static int is_utf8(char *c)
{
    int ascii=1;
    while(*c)
    {
        int p,n=(*c++) & 0xff;
        if (!(n & 0x80)) continue;
        ascii=0;
        if ((n & 0xe0)==0xc0) p=1;
        else if ((n & 0xf0)==0xe0) p=2;
        else if ((n & 0xf8)==0xf0) p=3;
        else if ((n & 0xfc)==0xf8) p=4;
        else if ((n & 0xfe)==0xfc) p=5;
        else return 0;
        for (;p && *c;p--) {
            n=((*c++) & 0xc0);
            if (n != 0x80) {
                return 0;
            }
        }
    }
    return !ascii;
}
#endif

static char *translate_phrase(mil_MilenaSimObject *self,char *phrase,int poststress)
{
    int n;
    n=milena_Prestresser(self->milena,phrase,self->buf1,self->blen1);
    if (n) {
        //printf("ral1\n");
        self->buf1=realloc(self->buf1,self->blen1=2*n);
        milena_Prestresser(self->milena,phrase,self->buf1,self->blen1);
    }
    n=milena_TranslatePhrase(self->milena,(unsigned char *)self->buf1,self->buf2,self->blen2,0);
    if (n) {
        //printf("ral2\n");
        
        self->buf2=realloc(self->buf2,self->blen2=2*n);
        milena_TranslatePhrase(self->milena,(unsigned char *)self->buf1,self->buf2,self->blen2,0);
    }
    if (!poststress) return self->buf2;
    n=milena_Poststresser(self->buf2,self->buf1,self->blen1);
    if (n) {
        //printf("ral3\n");
        
        self->buf1=realloc(self->buf1,self->blen1=2*n);
        milena_Poststresser(self->buf2,self->buf1,self->blen1);
    }
    return self->buf1;
    
}


static PyObject *MilenaSim_FilePath(mil_MilenaSimObject *self, PyObject *args)
{
    char *s;
    if (!PyArg_ParseTuple(args,"s",&s)) {
        return NULL;
    }
    milena_FilePath(s,self->buf1);
    if (!isfile(self->buf1)) {
        Py_RETURN_NONE;
    }
    return Py_BuildValue("s",self->buf1);
}

static int to_iso2_mp(char *instr,char *outstr,char **instrpos,int maxpos)
{
    int count_bad_char=0;
    int ignore_oor=1;
    return milena_utf2iso_mp(instr,outstr,ignore_oor,&count_bad_char,instrpos,maxpos);
}


static int to_iso2(char *instr,char *outstr)
{
        return to_iso2_mp(instr,outstr,NULL,0);
}


static int count_syllables(char *str)
{
        int syl=0;
        for (;*str;str++) {
                if (strchr("aeiouyYMOE",*str)) syl++;
        }
        return syl;
}

static int fins[]={1,0,1,1,0,0,0,0};
    
static PyObject *MilenaSim_FetchSentence(mil_MilenaSimObject *self,PyObject *args)
{
    char *txt;
    char *bdx,*edx,*phr;
    int this_ptyp,nlen,n;
    
    if (!PyArg_ParseTuple(args,"s",&txt)) {
        return NULL;
    }
    n=to_iso2(txt,NULL);
    if (n<=0) {
        Py_RETURN_NONE;
    }
    
        
    if (n>self->blen3) {
        //printf("ral4\n");
        self->buf3=realloc(self->buf3,self->blen3=2*n);
    }
    to_iso2(txt,self->buf3);
    to_iso2(txt,self->buf3);
    bdx=edx=self->buf3;
    for (;;) {
        nlen=milena_GetPhrase(self->milena,&edx,self->buf2,self->blen2,&this_ptyp);
        if (nlen < 0) {
            break;
        }
        if (nlen > 0) {
            //printf("ral5\n");
            
            self->buf2=realloc(self->buf2,self->blen2=2*nlen);
            milena_GetPhrase(self->milena,&edx,self->buf2,self->blen2,&this_ptyp);
        }
        if (fins[this_ptyp & 7]) break;
    }

    to_iso2_mp(txt,NULL,&edx,edx-bdx);
    return Py_BuildValue("(iss#)",this_ptyp,edx,txt,edx-txt);

}

static PyObject *MilenaSim_GetSentence(mil_MilenaSimObject *self,PyObject *args)
{
    char *txt;
    char *bdx,*edx,*phr;
    int this_ptyp,nlen,n;
    if (!PyArg_ParseTuple(args,"s",&txt)) {
        return NULL;
    }
    if (!self->ivoice) {
        get_error("Funkcja dopuszczalna wyłącznie w trybie Ivona");
        return NULL;
    }
    n=to_iso2(txt,NULL);
    if (n<=0) {
        Py_RETURN_NONE;
    }
    
        
    if (n>self->blen3) {
        //printf("ral4\n");
        self->buf3=realloc(self->buf3,self->blen3=2*n);
    }
    to_iso2(txt,self->buf3);
    bdx=edx=self->buf3;
    self->ivo_buf[0]=0;
    for (;;) {
        nlen=milena_GetPhrase(self->milena,&edx,self->buf2,self->blen2,&this_ptyp);
        if (nlen < 0) {
            break;
        }
        if (nlen > 0) {
            //printf("ral5\n");
            
            self->buf2=realloc(self->buf2,self->blen2=2*nlen);
            milena_GetPhrase(self->milena,&edx,self->buf2,self->blen2,&this_ptyp);
        }
        phr=translate_phrase(self,self->buf2,0);
        nlen =
#ifdef MILENA_IVO_HAS_SUBPHR
            milena_ivonizer_nb(this_ptyp,phr,self->ivo_buf,self->ivo_len,self->milena,self->bookmode);            
#else
            milena_ivonizer_n(this_ptyp,phr,self->ivo_buf,self->ivo_len,self->milena);
#endif
        if (nlen) {
            self->ivo_buf=realloc(self->ivo_buf,self->ivo_len=2*nlen+256);
#ifdef MILENA_IVO_HAS_SUBPHR
            milena_ivonizer_nb(this_ptyp,phr,self->ivo_buf,self->ivo_len,self->milena,self->bookmode);
#else
            milena_ivonizer_n(this_ptyp,phr,self->ivo_buf,self->ivo_len,self->milena);
#endif
        }
        if (fins[this_ptyp & 7]) break;
    }
    if (!self->ivo_buf[0]) {
        Py_RETURN_NONE;
    }
    to_iso2_mp(txt,NULL,&edx,edx-bdx);
    if (self->rhvoice) milena_ivo2rh(self->ivo_buf);
    return Py_BuildValue("(" STRING8 "iss#)",self->ivo_buf,this_ptyp,edx,txt,edx-txt);
}



static PyObject *MilenaSim_GetPhrase(mil_MilenaSimObject *self,PyObject *args)
{
    char *txt;
    int n,this_ptyp,nlen,plen;
    char *bdx,*edx,*phr;
    int pause=0;


    
    if (!PyArg_ParseTuple(args,"si",&txt,&pause)) {
        return NULL;
    }

    if (pause < 0 || pause > 5) {
        get_error("Nielegalny oddech spoza zakresu 0..5");
        return NULL;
    }
    if (self->ivoice) {
        get_error("Funkcja dopuszczalna tylko w trybie Mbrola");
        return NULL;
    }
    n=to_iso2(txt,NULL);
    if (n<=0) {
        Py_RETURN_NONE;
    }
    if (n<self->blen3) {
        //printf("ral7\n");
        
        self->buf3=realloc(self->buf3,self->blen3=2*n);
    }
    to_iso2(txt,self->buf3);
    bdx=edx=self->buf3;
    nlen=milena_GetPhrase(self->milena,&edx,self->buf2,self->blen2,&this_ptyp);
    if (nlen < 0) {
        Py_RETURN_NONE;
    }
    if (nlen > 0) {
        //printf("ral8\n");
        
        self->buf2=realloc(self->buf2,self->blen2=2*nlen);
        milena_GetPhrase(self->milena,&edx,self->buf2,self->blen2,&this_ptyp);
    }
    // w edx siedzi teraz koniec frazy
    // jakby trzeba było następną, to teraz
    if (self->bookmode && this_ptyp == 1) {
        int s_ptyp=0;
        char *t=edx;
        n=milena_GetPhrase(self->milena,&t,self->buf1,self->blen1,&s_ptyp);
        if ((s_ptyp & 7) == 2) {
            if (n>0) {
                //printf("ral9\n");
        
                self->buf1=realloc(self->buf1,self->blen1=2*n);
                milena_GetPhrase(self->milena,&t,self->buf1,self->blen1,&s_ptyp);
            }
            if (count_syllables(self->buf1) < 6) this_ptyp |= 128;
        }
    }
    // w buf2 siedzi sobie fraza przetworzona
    plen=edx-bdx;
    to_iso2_mp(txt,NULL,&edx,plen);
    phr=translate_phrase(self,self->buf2,1);
    self->phb.str_len=0;
    if (pause) {
        milena_ModMbrolaBreakP(self->mimbrola,&self->phb,pause);
    }
    milena_ModMbrolaGenPhraseP(self->mimbrola,phr,&self->phb,this_ptyp);
    // (out,pmode,rest,consumed)
    return Py_BuildValue("(siss#)",self->phb.str,this_ptyp,edx,txt,edx-txt);
}

static PyObject *MilenaSim_InsertDic(mil_MilenaSimObject *self,PyObject *args)
{
    char *txt,*fname;int lineno;
    if (!PyArg_ParseTuple(args,"ssi",&txt,&fname,&lineno)) {
        return NULL;
    }
#if PY_MAJOR_VERSION < 3
    if (is_utf8(txt)) {
#endif
        int n=to_iso2(txt,NULL);
        if (n<=0) {
            Py_RETURN_NONE;
        }
        if (n>self->blen3) {
            //printf("ral10\n");
        
            self->buf3=realloc(self->buf3,self->blen3=2*n);
        }
        to_iso2(txt,self->buf3);
        txt=self->buf3;
#if PY_MAJOR_VERSION < 3
    }
#endif
    if (!milena_ReadUserDicLineWithFlags(self->milena,txt,0,fname,lineno)) {
        get_error("Błędna linia słownika");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *MilenaSim_LoadDic(mil_MilenaSimObject *self,PyObject *args)
{
    char *txt;
    if (!PyArg_ParseTuple(args,"s",&txt)) {
        return NULL;
    }
    if (!milena_ReadUserDic(self->milena,txt)) {
        get_error("Błąd ładowania słownika użytkownika");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *MilenaSim_LoadMbrola(mil_MilenaSimObject *self,PyObject *args)
{
    char *txt;
    if (!PyArg_ParseTuple(args,"s",&txt)) {
        return NULL;
    }
    if (self->ivoice) {
        get_error("Funkcja dopuszczalna tylko w trybie Mbrola");
        return NULL;
    }
    if (!milena_ModMbrolaExtras(self->mimbrola,txt)) {
        get_error("Błąd ładowania pliku extras mbroli");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *MilenaSim_LoadPhraser(mil_MilenaSimObject *self,PyObject *args)
{
    char *txt;
    if (!PyArg_ParseTuple(args,"s",&txt)) {
        return NULL;
    }
    if (!milena_ReadPhraser(self->milena,txt)) {
        get_error("Błąd ładowania frazera");
        return NULL;
    }
    Py_RETURN_NONE;
}


static PyObject *MilenaSim_LoadLang(mil_MilenaSimObject *self,PyObject *args)
{
    char *txt;
    if (!PyArg_ParseTuple(args,"s",&txt)) {
        return NULL;
    }
    sprintf(self->buf1,"pl_%s_udic.dat",txt);
    milena_FilePath(self->buf1,self->buf2);
    if (isfile(self->buf2)) {
        if (!milena_ReadUserDic(self->milena,self->buf2)) {
            get_error("Błąd ładowania słownika języka");
            return NULL;
        }
    }
    sprintf(self->buf1,"pl_%s_stress.dat",txt);
    milena_FilePath(self->buf1,self->buf2);
    if (isfile(self->buf2)) {
        if (!milena_ReadStressFile(self->milena,self->buf2)) {
            get_error("Błąd ładowania pliku stress języka");
            return NULL;
        }
    }
    milena_SetLangMode(self->milena,txt);
    Py_RETURN_NONE;
}

static PyObject *MilenaSim_LoadTheme(mil_MilenaSimObject *self,PyObject *args)
{
    char *txt;
    if (!PyArg_ParseTuple(args,"s",&txt)) {
        return NULL;
    }
    sprintf(self->buf1,"pl_%s_theme.dat",txt);
    milena_FilePath(self->buf1,self->buf2);
    if (isfile(self->buf2)) {
        if (!milena_ReadPhraser(self->milena,self->buf2)) {
            get_error("Błąd ładowania tematu");
            return NULL;
        }
    }
    Py_RETURN_NONE;
}

static PyObject *MilenaSim_SetPitch(mil_MilenaSimObject *self,PyObject *args)
{
    double arg;
    if (!PyArg_ParseTuple(args,"d",&arg)) {
        return NULL;
    }
    if (self->ivoice) {
        get_error("Funkcja nie ma sensu dla Ivony");
        return NULL;
    }
    milena_ModMbrolaSetVoice(self->mimbrola,MILENA_MBP_PITCH,arg);
    Py_RETURN_NONE;
}

static PyObject *MilenaSim_SetRange(mil_MilenaSimObject *self,PyObject *args)
{
    double arg;
    if (!PyArg_ParseTuple(args,"d",&arg)) {
        return NULL;
    }
    if (self->ivoice) {
        get_error("Funkcja nie ma sensu dla Ivony");
        return NULL;
    }
    milena_ModMbrolaSetVoice(self->mimbrola,MILENA_MBP_RANGE,arg);
    Py_RETURN_NONE;
}

static PyObject *MilenaSim_AlNum(mil_MilenaSimObject *self,PyObject *args)
{
    int arg;
    if (!PyArg_ParseTuple(args,"i",&arg)) {
        return NULL;
    }
    return Py_BuildValue("i",milena_alnum(arg)?1:0);
}

static PyMethodDef MilenaSim_methods[]={
        {"filePath",(PyCFunction)MilenaSim_FilePath,METH_VARARGS,
                "filePath(name)\nZwraca ścieżkę do pliku danych Mileny lub None"},
        {"loadLang",(PyCFunction)MilenaSim_LoadLang,METH_VARARGS,
                "loadLang(name)\nŁaduje język"},
        {"loadTheme",(PyCFunction)MilenaSim_LoadTheme,METH_VARARGS,
                "loadTheme(name)\nŁaduje temat"},
        {"appendDicLine",(PyCFunction)MilenaSim_InsertDic,METH_VARARGS,
                "appendDicLine(line,fname,lineno)\nDodaje pojedynczą linię słownika"},
        {"loadDic",(PyCFunction)MilenaSim_LoadDic,METH_VARARGS,
                "loadDic(path)\nŁaduje słownik użytkownika"},
        {"loadPhraser",(PyCFunction)MilenaSim_LoadPhraser,METH_VARARGS,
                "loadPhraser(path)\nŁaduje frazer"},
        {"loadMbrola",(PyCFunction)MilenaSim_LoadMbrola,METH_VARARGS,
                "loadMbrola(path)\nŁaduje plik extras Mbroli"},
        {"getPhrase",(PyCFunction)MilenaSim_GetPhrase,METH_VARARGS,
                "getPhrase(text,breath)\nPobiera frazę z napisu UTF-8 dla Mbroli\nZwraca krotkę(phrase,mode,rest,consumed)"},
        {"fetchSentence",(PyCFunction)MilenaSim_FetchSentence,METH_VARARGS,
                "fetchSentence(text)\nPobiera zdanie z napisu UTF-8\nZwraca krotkę(mode,rest,consumed)"},
        {"getSentence",(PyCFunction)MilenaSim_GetSentence,METH_VARARGS,
                "getSentence(text)\nPobiera zdanie z napisu UTF-8 dla Ivony\nZwraca krotkę(phrase,mode,rest,consumed)"},
        {"setPitch",(PyCFunction)MilenaSim_SetPitch,METH_VARARGS,
                "setPitch(value)\nUstala wysokość tonu mbroli (normalnie 1.0)"},
        {"setRange",(PyCFunction)MilenaSim_SetRange,METH_VARARGS,
                "setRange(value)\nUstala szerokość melodii mbroli (normalnie 1.0)"},
        {"isAlNum",(PyCFunction)MilenaSim_AlNum,METH_VARARGS,
                "isAlNum(code)\nZwraca wartość milena_alnum dla danego kodu Unicode"},
        {NULL,NULL,0,NULL}
};


static void MilenaSim_dealloc(mil_MilenaSimObject *self)
{
    freeme(self);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyTypeObject mil_MilenaSimType = {
#if PY_MAJOR_VERSION == 3
    PyVarObject_HEAD_INIT(NULL, 0)
#else
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
#endif
    "milenasimple.Milenasimple",             /*tp_name*/
    sizeof(mil_MilenaSimObject), /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)MilenaSim_dealloc,                         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                                /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /*tp_flags*/
    "Simple milenka object:\nMilenasimple(flags,ivoice=None)",           /* tp_doc */
    0,                               /* tp_traverse */
    0,                               /* tp_clear */
    0,                               /* tp_richcompare */
    0,                               /* tp_weaklistoffset */
    0,                               /* tp_iter */
    0,                               /* tp_iternext */
    MilenaSim_methods,         /* tp_methods */
    0,                         /* tp_members */
    0, //MilenaSim_getseters,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)MilenaSim_init,  /* tp_init */
};


static PyMethodDef MilenaSim_ModMethods[]={
        {"filePath",(PyCFunction)MilenaSim_FilePath,METH_VARARGS,
                "filePath(str)\nZwraca ścieżkę do pliku danych Mileny lub None"},
        {"isAlNum",(PyCFunction)MilenaSim_AlNum,METH_VARARGS,
                "isAlNum(int)\nZwraca wartość milena_isalnum dla danego kodu Unicode"},
        {NULL,NULL,0,NULL}
};


#ifndef PyMODINIT_FUNC        /* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif

MOD_INIT(milenasimple)
{
    PyObject* m; int i;

    mil_MilenaSimType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&mil_MilenaSimType) < 0)
        return MOD_RETVAL;

    MOD_DEF(m,"milenasimple",
                       "milenasimple - simple interface to Milena Natural Language Processor",
                       MilenaSim_ModMethods);
    Py_INCREF(&mil_MilenaSimType);
    PyModule_AddObject(m, "Milenasimple", (PyObject *)&mil_MilenaSimType);
    for (i=0;milena_consts[i].name;i++) {
        PyModule_AddObject(m, milena_consts[i].name, Py_BuildValue("i",milena_consts[i].value));
    }
    PyModule_AddObject(m,"milena_version",Py_BuildValue("s",milena_GetVersion()));
    PyModule_AddObject(m,"milena_data",Py_BuildValue("s",milena_GetDataPath()));
#if PY_MAJOR_VERSION >= 3
    return m;
#endif
    
}
