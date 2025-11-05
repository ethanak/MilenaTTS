#!/usr/bin/env python
#coding: utf-8

from milenasimple import *
import sys


class Milena(Milenasimple):
    """
    Interfejs wyższego poziomu do procesora języka Milena
    """
    def __init__(self,flags=None,ivoice=None,
                 langs=None,
                 themes=None,
                 dic=None,
                 phraser=None,
                 mbrola=None,
                 rhvoice=False,
                 ivocoding='utf-8'):
        self._rh=rhvoice
        if rhvoice:
            ivocoding='unicode'
            ivoice='Ewa'
        self._enc=ivocoding.lower()
        self._flags=0
        self._paragraph=None
        self._last_partype=-1
        self._ivoice=ivoice

        self._corrector=None
        
        minit={}
        if flags:
            minit['flags']=flags
            self._flags=flags
        if ivoice:
            minit['ivoice']=ivoice
        if rhvoice:
            minit['rhvoice']=1
        Milenasimple.__init__(self,**minit)
        if langs:
            if isinstance(langs,str):
                langs=[langs]
            for lang in langs:
                self.loadLang(lang)
        if themes:
            if isinstance(themes,str):
                themes=[themes]
            for theme in themes:
                self.loadTheme(theme)
        if dic:
            if isinstance(dic,str):
                dic=[dic]
            for a in dic:
                self.loadDic(a)
        if phraser:
            if isinstance(phraser,str):
                phraser=[phraser]
            for a in phraser:
                self.loadPhraser(a)
        if mbrola:
            if isinstance(mbrola,str):
                mbrola=[mbrola]
            for a in mbrola:
                self.loadMbrola(a)
        
    def getPhrase(self,text,breath=0):
        """getPhrase(text,breath=0)
Pobiera frazę dla Mbroli z wprowadzonego tekstu. Jeśli breath != 0,
wyjście poprzedzane jest fonemem pauzy odpowiadającej długości
oddechu.
Zwracana wartość:
krotka (fonemy,typ_frazy,reszta_tekstu,przeczytany_tekst)
lub None jeśli koniec tekstu.
W Pythonie 2 jeśli parametr text jest unikodem, trzecia i czwarta
wartość krotki wynikowej będą również unikodem.
"""
        if (sys.version_info[0] == 3) or not isinstance(text,unicode):
            return Milenasimple.getPhrase(self,text,breath)
        text=text.encode('utf-8')
        d=Milenasimple.getPhrase(self,text,breath)
        if d:
            d=(d[0],d[1],d[2].decode('utf-8') if d[2] is not None else None,d[3].decode('utf-8'))
        return d

    def getSentence(self,text):
        
        """getSentence(text)
Pobiera fonetyczną wersję zdania z wprowadzonego tekstu.
Zwracana wartość:
krotka (fonemy,typ_frazy,reszta_tekstu,przeczytany_tekst)
lub None jeśli koniec tekstu.
W Pythonie 2 jeśli parametr text jest unikodem, trzecia i czwarta
wartość krotki wynikowej będą również unikodem.
Kodowanie fonemów określa parametr ivocoding inicjalizatora klasy,
dopuszczalne wartości to: 'utf-8', 'iso-8859-2', 'unicode'.
W Pythonie 3 jeśli ivocoding nie jest 'unicode', fonemy
w krotce będą typu bytes.
"""
        is_uni=False
        if (sys.version_info[0] < 3) and isinstance(text,unicode):
            is_uni=True
            text=text.encode('utf-8')
        d=Milenasimple.getSentence(self,text)
        if not d:
            return None
        if self._enc == 'utf-8':
            #if self._rh:
            #    d=(milenasimple.iv(d[0].decode('iso-8859-2')).encode('utf-8'),d[1],d[2],d[3])
            #else:
            d=(d[0].decode('iso-8859-2').encode('utf-8'),d[1],d[2],d[3])
        elif self._enc == 'unicode':
            #if self._rh:
            #    d=(self._rhize(d[0].decode('iso-8859-2')),d[1],d[2],d[3])
            #else:
            d=(d[0].decode('iso-8859-2'),d[1],d[2],d[3])
        if is_uni:
            d=(d[0],d[1],d[2].decode('utf-8'),d[3].decode('utf-8'))
        return d
    
    def getParType(self,text):
        if (sys.version_info[0] < 3) and not isinstance(text,unicode):
            text=text.decode('utf-8')
        text=text.strip()
        if not text:
            return PARTYPE_EMPTY;
        dialog=PARTYPE_NORMAL
        if text[0] == '-' and len(text)> 1 and text[1].isdigit():
            return PARTYPE_NORMAL
        if text[0] in u"-\x0212\x0213\x0214\x0215":
            text=text[1:]
            dialog=PARTYPE_DIALOG
        for a in text:
            if self.isAlNum(ord(a)):
                return dialog
        return PARTYPE_EMPTY
    
    def feed(self,text=None):
        """feed(text)
Parametr text musi być pojedynczym akapitem lub None.
W przypadku None zerowane są wewnętrzne zmienne i zwracane jest None.
Inaczej wewnętrzny tekst inicjalizowany jest podanym tekstem,
określany jest typ akapitu oraz na podstawie bieżącego i poprzedniego
typu określany jest oddech. Zwracana jest krotka (typ,oddech).
"""
        if text is None:
            self._paragraph=None
            self._last_partype=-1
            return None
        pt=self.getParType(text)
        breath=0
        if pt == PARTYPE_EMPTY:
            self._paragraph=None
        else:
            self._paragraph=text
            if self._last_partype >= 0:
                if self._last_partype == PARTYPE_EMPTY:
                    breath=BREATH_LONG
                elif self._last_partype == PARTYPE_DIALOG:
                    breath=BREATH_DIALOG if pt == PARTYPE_DIALOG else BREATH_POSTDIAL
                else:
                    breath=BREATH_NORMAL if pt == PARTYPE_NORMAL else BREATH_PREDIAL
        self._last_partype = PARTYPE_EMPTY
        self._breath=breath
        return (self._last_partype,breath)
    
    def consume(self):
        """consume()
Pobiera z wewnętrznego tekstu frazę (Mbrola) lub zdanie (Ivona, RHVoice).
Zwraca krotkę (fonemy, przeczytany_tekst, typ_frazy)
lub None po przeczytaniu całego akapitu.
W przypadku Mbroli pierwsza fraza będzie poprzedzona odpowiednim
oddechem.
"""
        if not self._paragraph:
            return None
        breath=self._breath
        self._breath=0
        if self._ivoice:
            d=self.getSentence(self._paragraph)
        else:
            d=self.getPhrase(self._paragraph,breath)
        if not d:
            self._paragraph=None
            return None
        self._paragraph=d[2]
        return (d[0],d[3],d[1])

    def consumeSentence(self):
        """consumeSentence()
Pobiera z wewnętrznego tekstu zdanie bez tłumaczenia na fonemy.
Zwraca pobrane zdanielub None po przeczytaniu całego akapitu.
        """
        if not self._paragraph:
            return None
        d=self.fetchSentence(self._paragraph)
        if not d:
            self._paragraph=None
            return None
        self._paragraph=d[1]
        return d[2].strip()

    def splitText(self, text=None):
        """splitText(text)
splitText()
Dzieli na akapity i zdania podany tekst lub zawartość
wewnętrznego bufora. Zwraca listę akapitów lub None.
"""
        if text is None:
            text = self._paragraph
            if text is None:
                return None
            self._paragraph = None
        rc=list(x.strip() for x in text.split('\n') if x.strip() != '')
        if len(rc) == 0:
            return None
        for i,t in enumerate(rc):
            rc[i] = self.splitParagraph(t)
        return rc

    @property
    def corrector(self):
        if not self._corrector:
            import regex
            self._corrector = {
                'tdash': regex.compile(r'^(.*)([-―—−–]\s*)$'),
                'tsend': regex.compile(r'''^(([^\pL\d](?![\pL\d])|\s)+)(.*)$'''),
                'fdall': regex.compile(r'([\pL\d]+(?:[-―—−–.][\pL\d]+)*)'),
                'dshes': regex.compile(r'[―—−–]')
            }
        return self._corrector

    def getWordList(self, text):
        """getWordList(text):
Pobiera listę słów z tekstu
"""
        r=self.corrector['fdall'].findall(self.corrector['dshes'].sub('-',text))
        if r:
            r=list(x for x in r if x != '')
            if len(r) == 0:
                return None
            return r
        return None

    
    def splitParagraph(self, text=None,correct=False):
        """splitParagraph(text=None, correct=False)
Dzieli na zdania podany tekst lub zawartość wewnętrznego bufora.
Jeśli correct jest True, stara się skorygować listę zdań
uwzględniając interpunkcję. Zwraca listę zdań lub None.
"""
        if text is None:
            text = self._paragraph
            if text is None:
                return None
            self._paragraph = None
        rc=[]
        while True:
            d=self.fetchSentence(text)
            if not d:
                break
            rc.append(d[2].lstrip())
            text = d[1]
        if len(rc) == 0:
            return None
        if correct:
            if len(rc) == 1:
                return rc
            ps=[]
            bx=rc.pop(0)
            while rc:
                bt=rc.pop(0)
                r=self.corrector['tdash'].match(bx)
                if r:
                    bx=r.group(1)
                    bt=r.group(2)+bt
                else:
                    r=self.corrector['tsend'].match(bt)
                    if r:
                        bx = bx + r.group(1)
                        bt = r.group(3)
                    
                ps.append(bx)
                bx=bt
            ps.append(bx)
            return list(x for x in ps if x != '')
        return rc
        
    def addPhraserText(self, text):
        """
        addPhrasertext(text)
Dodaje słownik frazera w postaci tekstu        
        """
        text=text.encode('utf-8')
        import tempfile
        f=tempfile.NamedTemporaryFile(prefix='pl_phraser_',suffix='.dat')
        f.write(text)
        f.seek(0)
        self.loadPhraser(f.name)

    def addTLD(self, tld):
        """
        addTLD(tld)
Dodaje do frazera jedną (w przypadku parametru typu str) lub więcej
(w przypadku listy) top level domains. Przydatne praktycznie
tylko do podziału na zdania
        """
        if isinstance(tld, str):
            tld=[tld]
        tld='\n'.join('tld %s' % x for x in tld)+'\n'
        self.addPhraserText(tld)
        
        
        
