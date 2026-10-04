#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#define CLS "cls"

void init(void){}
unsigned long ms(void){ return GetTickCount(); }
void delay(unsigned int x){ Sleep(x); }
int key(void){ return _kbhit(); }
int get(void){ return _getch(); }

#else

#include <unistd.h>
#include <termios.h>
#include <sys/select.h>

#define CLS "clear"

struct termios old;

void restore(void)
{
    tcsetattr(0,TCSANOW,&old);
}

void init(void)
{
    struct termios t;

    tcgetattr(0,&old);
    t=old;

    t.c_lflag &= ~(ICANON|ECHO);
    t.c_cc[VMIN]=1;
    t.c_cc[VTIME]=0;

    tcsetattr(0,TCSANOW,&t);
    atexit(restore);
}

unsigned long ms(void)
{
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC,&t);

    return t.tv_sec*1000UL+t.tv_nsec/1000000UL;
}

void delay(unsigned int x)
{
    usleep(x*1000);
}

int key(void)
{
    fd_set f;
    struct timeval t={0,0};

    FD_ZERO(&f);
    FD_SET(0,&f);

    return select(1,&f,NULL,NULL,&t)>0;
}

int get(void)
{
    unsigned char c;

    return read(0,&c,1)==1 ? c : -1;
}

#endif

#define N 4
#define TRIALS 8
#define ROUNDS 5

void clear(void)
{
    system(CLS);
}

void enter(void)
{
    int c;

    do {
        c=get();
    } while(c!='\n' && c!='\r');
}

void flush(void)
{
    while(key())
        get();
}

int rnd(int a,int b)
{
    return a+rand()%(b-a+1);
}

/* ================= GAME 1 ================= */

void reaction(void)
{
    double a[TRIALS],sum=0,sq=0;
    double mean,sd;

    int i,n=0;
    int early,lapse=0;

    clear();

    printf("================================\n");
    printf("       GAME 1 : REACTION TEST\n");
    printf("================================\n\n");

    printf("Press SPACE when GO appears.\n");
    printf("Do not press before GO.\n\n");

    printf("Press ENTER to start...");
    fflush(stdout);
    enter();

    i=0;

    while(i<TRIALS)
    {
        unsigned long t0,start,t=0;
        int hit=0;

        early=0;

        clear();

        printf("Trial %d/%d\n\n",i+1,TRIALS);
        printf("Wait for GO...\n");

        fflush(stdout);
        flush();

        t0=ms();

        while(ms()-t0<(unsigned long)rnd(1500,4000))
        {
            if(key())
            {
                get();
                early=1;
                break;
            }

            delay(5);
        }

        if(early)
        {
            printf("\nToo early! Try again.\n");
            delay(1000);
            continue;
        }

        clear();

        printf("\n\n       ***** GO! *****\n");
        fflush(stdout);

        start=ms();

        while(ms()-start<3000)
        {
            if(key())
            {
                get();
                t=ms()-start;
                hit=1;
                break;
            }

            delay(1);
        }

        if(hit)
        {
            a[n++]=t;

            if(t>1000)
                lapse++;

            printf("\nReaction time: %lu ms\n",t);
        }
        else
        {
            a[n++]=3000;
            lapse++;

            printf("\nMissed!\n");
        }

        i++;

        delay(800);
    }

    for(i=0;i<n;i++)
        sum+=a[i];

    mean=sum/n;

    for(i=0;i<n;i++)
        sq+=(a[i]-mean)*(a[i]-mean);

    sd=sqrt(sq/n);

    printf("\nMean reaction time: %.0f ms\n",mean);
    printf("Reaction variation: %.0f ms\n",sd);
    printf("Slow/missed trials: %d\n",lapse);

    printf("\nPress ENTER for Game 2...");
    fflush(stdout);
    enter();
}

/* ================= GAME 2 ================= */

void rotate(int a[N][N],int b[N][N])
{
    int i,j;

    for(i=0;i<N;i++)
        for(j=0;j<N;j++)
            b[i][j]=a[N-1-j][i];
}

void mirror(int a[N][N],int b[N][N])
{
    int i,j;

    for(i=0;i<N;i++)
        for(j=0;j<N;j++)
            b[i][j]=a[i][N-1-j];
}

void copygrid(int a[N][N],int b[N][N])
{
    memcpy(b,a,sizeof(int)*N*N);
}

int same(int a[N][N],int b[N][N])
{
    return memcmp(a,b,sizeof(int)*N*N)==0;
}

void printrow(int a[N][N],int r)
{
    int j;

    for(j=0;j<N;j++)
        printf(a[r][j]?"[#]":"[ ]");
}

int spatial(void)
{
    int score=0,r;

    clear();

    printf("================================\n");
    printf("    GAME 2 : SPATIAL PATTERN\n");
    printf("================================\n\n");

    printf("Choose the pattern rotated CLOCKWISE.\n");
    printf("Answer A, B or C.\n\n");

    printf("Press ENTER to start...");
    fflush(stdout);
    enter();

    for(r=1;r<=ROUNDS;r++)
    {
        int o[N][N],rot[N][N];
        int m[N][N],d[N][N];
        int op[3][N][N];

        int i,j,c,pos,slot=0,count;

        do
        {
            count=0;

            for(i=0;i<N;i++)
            {
                for(j=0;j<N;j++)
                {
                    o[i][j]=(rand()%100<45);
                    count+=o[i][j];
                }
            }

            rotate(o,rot);
            mirror(rot,m);
            copygrid(rot,d);

            i=rand()%N;
            j=rand()%N;

            d[i][j]=!d[i][j];

        }while(
            count<5 || count>10 ||
            same(rot,m) ||
            same(m,d) ||
            same(rot,o) ||
            same(m,o) ||
            same(d,o)
        );

        pos=rand()%3;

        for(i=0;i<3;i++)
        {
            if(i==pos)
                copygrid(rot,op[i]);
            else
            {
                copygrid(slot?d:m,op[i]);
                slot++;
            }
        }

        clear();

        printf("Round %d/%d\n\n",r,ROUNDS);
        printf(" ORIGINAL       A       B       C\n\n");

        for(i=0;i<N;i++)
        {
            printf(" ");
            printrow(o,i);
            printf("   ");

            for(j=0;j<3;j++)
            {
                printrow(op[j],i);
                printf("   ");
            }

            printf("\n");
        }

        printf("\nAnswer: ");
        fflush(stdout);

        do
        {
            c=toupper(get());
        }
        while(c!='A' && c!='B' && c!='C');

        printf("%c\n",c);

        if(c-'A'==pos)
        {
            score++;
            printf("Correct!\n");
        }
        else
        {
            printf(
                "Wrong! Correct answer: %c\n",
                'A'+pos
            );
        }

        delay(1000);
    }

    return score;
}

/* ================= MAIN ================= */

int main(void)
{
    int score;
    char again;

    init();

    srand((unsigned)time(NULL));

    do
    {
        clear();

        printf("====================================\n");
        printf("       COGNITIVE SCREENING GAME\n");
        printf("====================================\n\n");

        printf("Games:\n");
        printf("1. Reaction Test\n");
        printf("2. Spatial Pattern Matching\n\n");

        printf("Press ENTER to start...");
        fflush(stdout);

        enter();

        reaction();

        score=spatial();

        clear();

        printf("================================\n");
        printf("            RESULTS\n");
        printf("================================\n\n");

        printf(
            "Spatial score: %d/%d\n",
            score,ROUNDS
        );

        printf("\nThank you for playing!\n");

        printf("\nPlay again? (Y/N): ");
        fflush(stdout);

        do
        {
            again=toupper(get());
        }
        while(again!='Y' && again!='N');

    }
    while(again=='Y');

    clear();

    return 0;
}
