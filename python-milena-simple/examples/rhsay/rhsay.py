#!/usr/bin/env python3

from rhvoice_wrapper import TTS
import pyaudio, sys, os, getopt
import milena,contextlib

@contextlib.contextmanager
def ignoreStderr():
    devnull = os.open(os.devnull, os.O_WRONLY)
    old_stderr = os.dup(2)
    sys.stderr.flush()
    os.dup2(devnull, 2)
    os.close(devnull)
    try:
        yield
    finally:
        os.dup2(old_stderr, 2)
        os.close(old_stderr)

@contextlib.contextmanager
def ignoreStdAll():
    devnull = os.open(os.devnull, os.O_WRONLY)
    old_stderr = os.dup(2)
    old_stdout = os.dup(1)
    sys.stderr.flush()
    sys.stdout.flush()
    os.dup2(devnull, 2)
    os.dup2(devnull, 1)
    os.close(devnull)
    try:
        yield
    finally:
        sys.stdout.flush()
        sys.stderr.flush()
        os.dup2(old_stderr, 2)
        os.close(old_stderr)
        os.dup2(old_stdout, 1)
        os.close(old_stdout)

params = {}

def helpme():
    print("""Sposób użycia:
%s [parametry] [tekst do powiedzenia]
gdzie parametry to:
  -h - wyświetlenie pomocy
  -m - wyłączenie Mileny (automatycznie dla języków poza polskim)
  -i <nazwa> - nazwa pliku wejściowego lub znak - dla stdin
  -o <nazwa> - nazwa pliku wynikowego lub znak - dla stdout
  -v <głos>  - nazwa głosu
  -t <typ>   - typ pliku wynikowego (raw, pcm, wav, mp3, opus, flac)
  -p <pitch> - wysokość głosu 0.5 .. 1.6
  -s <speed> - szybkość wymowy 0.5 .. 1.6

Jeśli nie będzie podany typ wynikowy, będzie określony na podstawie
    rozszerzenia nazwy. Typ raw i pcm to to samo. W przypadku stdout
    domyślnym typem jest pcm.
Jeśli nie będzie podany plik wynikowy, mowa będzie przekazana do wyjścia
    audio.
Jeśli nie będzie podany plik wejściowy ani tekst do powiedzenia,
    program będzie czytał tekst z stdin.
Jeśli będą podane oba (plik wejściowy i tekst), odczytany zostanie
    najpierw tekst, a potem wejście.""" % sys.argv[0])
    
    exit(1)
    
class lektor(object):
    gopt='i:o:v:s:p:t:hm'
    tps=('pcm','raw','wav','opus','mp3','flac')
    def __init__(self, args = None):
        self.params= {
            'voice': 'natan',
            'speed': 1.0,
            'pitch': 1.0,
            'infile': None,
            'outfile': None,
            'text': None,
            'type': None,
            'milena': True
            
        }
        if args is not None:
            self.parseargs(args)

    def read(self):
        pre = self.params['text']
        if pre is not None:
            self.params['text'] = pre+'.\n'+self.params['infile'].read()
        else:
            self.params['text'] = self.params['infile'].read()
        print(self.params['text'])

    def run(self):
        self.m=milena.Milena(rhvoice=True) if self.params['milena'] else None
        if self.params['infile'] is not None:
            self.read()
        self.text=self.params['text']
        with ignoreStdAll():
            self.tts = TTS(threads=1)
        self.tts.set_params(
            relative_pitch = self.params['pitch'],
            relative_rate = self.params['speed'])
        if self.params['milena']:
            self.fall=[]
            while res := self.m.getSentence(self.text):
                self.fall.append(res[0])
                self.text = res[2]
            self.fall = ' '.join(self.fall)
        else:
            self.fall = self.text
            self.text=None
        if self.params['outfile'] is None:
            return self.say()
        else:
            return self.store()

    def store(self):
        try:
            if self.params['outfile'] != '-':
                self.tts.to_file(voice = self.params['voice'],format_=self.params['type'], filename = self.params['outfile'],text=self.fall)
            else:
            
                fmt = self.params['type']
                if fmt is None:
                    fmt = 'pcm'
                with self.tts.say(self.fall, voice=self.params['voice'], format_=fmt) as gen:
                    for chunk in gen:
                        sys.stdout.buffer.write(chunk)
                sys.stdout.flush()
        except:
            pass
        self.tts.join()
        exit(0)
        
    def say(self):
        with ignoreStdAll():
            audio=pyaudio.PyAudio()
        try:
            with self.tts.say(self.fall, voice=self.params['voice']) as gen:
                started = False
                for chunk in gen:
                    if not started:
                        started=True
                        rate=chunk[24]+chunk[25]*256
                        chunk=chunk[44:]
                        stream = audio.open(format = audio.get_format_from_width(2),
                                channels = 1,
                                rate = rate,
                                output = True)
                    stream.write(chunk)
        except:
            pass
        audio.terminate()
        self.tts.join()
        exit(0)

    def err(self, *txt):
        print('Błąd: ',' '.join(txt), file = sys.stderr)
        helpme()
        
    def parseargs(self, args):
        try:
            optlist, args = getopt.getopt(args, self.__class__.gopt)
        except Exception as e:
            self.err(str(e))
        if len(args) > 0:
            self.params['text'] = ' '.join(args)
        ov=set()
        for a in optlist:
            opt = a[0]
            opv = a[1] if len(a) > 0 else None
            if opt == '-h':
                helpme()
            if opt in ov:
                self.err("Powtórzona opcja", opt)
            ov.add(opt)
            
            if opt == '-m':
                self.params['milena'] = False
            elif opt == '-i':
                if opv != '-' and not os.path.isfile(opv):
                    self.err("Brak pliku", opv)
                self.params['infile'] = open(opv,'r') if opv != '-' else sys.stdin
            elif opt in ('-s','-p'):
                try:
                    opv=float(opv)
                    if opv < 0.5 or opv > 1.6:
                        raise Exception()
                except:
                    self.err("Parametr dla", opt,"musi być float z zakresu 0.5 do 1.5")
                if opt == '-s':
                    self.params['speed'] = opv
                else:
                    self.params['pitch'] = opv
            elif opt == '-v':
                self.params['voice'] = opv
            elif opt == '-t':
                if opv not in self.__class__.tps:
                    self.err("Błędny typ")
                self.params['type'] = 'pcm' if opv == 'raw' else opv
            elif opt == '-o':
                self.params['outfile'] = opv
        if self.params['text'] is None and self.params['infile'] is None:
            self.params['infile'] = sys.stdin

        if self.params['voice'].lower() not in ('natan','magda','michal','alicja','cezary'):
            self.params['milena'] = False

        if self.params['outfile'] is not None and self.params['type'] is None and self.params['outfile'] != '-':
            opv = self.params['outfile'].split('.')[-1].lower()
            if opv not in self.__class__.tps:
                self.err("Błędny typ")
            self.params['type'] = 'pcm' if opv == 'raw' else opv


l=lektor(sys.argv[1:])
l.run()
