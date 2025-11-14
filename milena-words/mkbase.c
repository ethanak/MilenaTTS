#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>


char *trim(char *s)
{
	char *d,*e;
	while (*s && isspace(*s)) s++;
	for (d=e=s;*d;d++) if (!isspace(*d)) e=d+1;
	*e=0;
	return s;
}


char *morf_file;
char *base_infile;
char *base_outfile;


#define BLK_SIZE (1024*1024*1024)
#define WRD_CNT 8192

static int blk_end;
static char *blok;
static char **words;
static int wordcnt,wordsiz;

static struct delword {
	struct delword *next;
	char *wrd;
} *dwords;

static void morf_del_word(char *c)
{
	struct delword *dw;
	dw=malloc(sizeof(*dw));
	dw->next=dwords;
	dwords=dw;
	dw->wrd=strdup(c);
}


static int strxcmp(char *pat,char *str)
{
	while (*pat) {
		if (*pat=='*') return 0;
		if (*pat++ != *str++) return 1;
	}
	return *str;
}
static int is_delword(char *s)
{
	struct delword *dw;
	for (dw=dwords;dw;dw=dw->next) {
		if (!strxcmp(dw->wrd,s)) {
			printf("Deleted %s\n",s);
			return 1;
		}
	}
	return 0;
}

static void morf_add_word(char *c)
{
	int siz=strlen(c)+1;
	if (*c=='-') {
		morf_del_word(c+1);
		return;
	}
	if (!blok || blk_end+siz>BLK_SIZE) {
		blok=malloc(BLK_SIZE);
		if (!blok) exit(1);
		blk_end=0;
	}
	if (!words) {
		words=malloc(sizeof(*words) * (wordsiz=WRD_CNT));
	}
	else if (wordcnt>=wordsiz) {
		words=realloc(words,sizeof(*words) * (wordsiz=wordsiz+WRD_CNT));
	}
	words[wordcnt++]=blok+blk_end;
	strcpy(blok+blk_end,c);
	blk_end+=siz;
}

void read_base()
{
	int fd,i;
	size_t len;
	char *bdy;
	struct stat sb;
	if (stat(base_infile,&sb)) {
		perror(base_infile);
		exit(1);
	}
	len=sb.st_size;
	fd=open(base_infile,O_RDONLY);
	if (fd<0) {
		perror(base_infile);
		exit(1);
	}
	
	bdy=malloc(len);
	if (read(fd,bdy,len)!=len) {
		perror("Base read");
		exit(1);
	}
	close(fd);
	wordcnt=wordsiz=strtol(bdy,NULL,10);
	bdy+=10;
	words=malloc(wordcnt*sizeof(*words));
	for (i=0;i<wordcnt;i++) {
		words[i]=bdy;
		bdy+=strlen(bdy)+1;
	}
	fprintf(stderr,"Base OK, wczytano %d slow\n",wordcnt);
}

static int morf_strcmp(const void *s1,const void *s2)
{
	return strcmp(*(char **)s1,*(char **)s2);
}

void store_base()
{
	FILE *f;
	int i,j;char *c;
	char buf[10];
	qsort(words,wordcnt,sizeof(*words),morf_strcmp);
	fprintf(stderr,"Posortowane\n");
	for (i=j=0,c="";i<wordcnt;i++) {
		if (is_delword(words[i])) continue;
		if (strcmp(words[i],c)) {
			words[j++]=c=words[i];
		}
	}
	printf("%d %d\n",i,j);
	wordcnt=j;
	f=fopen(base_outfile,"w");
	if (!f) {
		perror(base_outfile);
		exit(1);
	}
	sprintf(buf,"%9d",wordcnt);
	fwrite(buf,10,1,f);
	for (i=0;i<wordcnt;i++) {
		fwrite(words[i],strlen(words[i])+1,1,f);
	}
	fclose(f);
}

void usage(char *s)
{
	fprintf(stderr,"%s [-b baza_old] -o baza_new [plik...]\n",s);
	exit(1);
}

void read_wordfile(char *fname)
{
	FILE *f;
	int n=0;
	char buf[1024],*c,*w;
	fprintf(stderr,"Reading %s\n",fname);
	f=fopen(fname,"r");
	if (!f) {
		perror(fname);
		exit(1);
	}
	while (fgets(buf,1024,f)) {
		c=strstr(buf,"//");
		if (c) *c=0;
		w=trim(buf);
		if (!*w) continue;
		for (c=buf;*c;c++) if (isspace(*c)) break;
		if (*c) continue;
		n++;
		morf_add_word(w);
	}
	fprintf(stderr,"Dodane %d\n",n);
	fclose(f);
}


int main(int argc,char *argv[])
{
	for(;;) {
		int c=getopt(argc,argv,"b:o:");
		if (c<0) break;
		else if (c=='b') base_infile=optarg;
		else if (c=='o') base_outfile=optarg;
		else usage(argv[0]);
	}
	if (!base_outfile) usage(argv[0]);
	if (!base_infile) {
		base_infile="/usr/share/milena-words/pl_basewords.dat";
	}
	read_base();
	while(optind < argc) read_wordfile(argv[optind++]);
	store_base();
}
