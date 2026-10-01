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
#define CLS_CMD "cls"
#else
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#define CLS_CMD "clear"
#endif
 
#define N 4
#define RT_TRIALS 8
#define SPATIAL_ROUNDS 5
#define SPAN_ROUNDS 4
#define SEARCH_ROUNDS 4
#define SEARCH_ROWS 5
#define SEARCH_COLS 12
#define SEARCH_TIME_LIMIT_MS 20000UL
 
/* ------------------------------------------------------------
 *  Platform layer (timer, sleep, keyboard)
 * ------------------------------------------------------------ */
#ifdef _WIN32
 
static void init_terminal(void) { }
 
static unsigned long now_ms(void)
{
    return (unsigned long)GetTickCount();
}
 
static void sleep_ms(unsigned int ms)
{
    Sleep(ms);
}
 
static int key_pressed(void)
{
    return _kbhit();
}
 
static int get_key(void)
{
    return _getch();
}
 
#else
 
static struct termios saved_term;
static int term_saved = 0;
 
static void restore_terminal(void)
{
    if (term_saved) {
        tcsetattr(0, TCSANOW, &saved_term);
    }
}
 
static void init_terminal(void)
{
    struct termios raw;
    if (tcgetattr(0, &saved_term) == 0) {
        term_saved = 1;
        raw = saved_term;
        raw.c_lflag &= ~(ICANON | ECHO);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(0, TCSANOW, &raw);
        atexit(restore_terminal);
    }
}
 
static unsigned long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long)(ts.tv_sec * 1000UL + ts.tv_nsec / 1000000UL);
}
 
static void sleep_ms(unsigned int ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}
 
static int key_pressed(void)
{
    fd_set fds;
    struct timeval tv;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    return select(1, &fds, NULL, NULL, &tv) > 0;
}
 
static int get_key(void)
{
    unsigned char c;
    if (read(0, &c, 1) == 1) {
        return (int)c;
    }
    return -1;
}
 
#endif
 
/* ------------------------------------------------------------
 *  Generic helpers
 * ------------------------------------------------------------ */
static void clear_screen(void)
{
    (void)system(CLS_CMD);
}
 
static void flush_keys(void)
{
    while (key_pressed()) {
        get_key();
    }
}
 
static void wait_enter(void)
{
    int c;
    for (;;) {
        c = get_key();
        if (c == '\r' || c == '\n') {
            break;
        }
    }
}
 
/* Read a line of text using raw key input (works the same everywhere). */
static void read_line(char *buf, int max)
{
    int len = 0;
    int c;
    for (;;) {
        c = get_key();
#ifdef _WIN32
        if (c == 0 || c == 224) {   /* special key prefix: skip next code */
            get_key();
            continue;
        }
#endif
        if (c == '\r' || c == '\n') {
            putchar('\n');
            break;
        }
        if ((c == 8 || c == 127) && len > 0) {
            len--;
            printf("\b \b");
            fflush(stdout);
            continue;
        }
        if (c >= 32 && c < 127 && len < max - 1) {
            buf[len++] = (char)c;
            putchar(c);
            fflush(stdout);
        }
    }
    buf[len] = '\0';
}
 
static int rand_range(int lo, int hi)
{
    return lo + rand() % (hi - lo + 1);
}
 
/* ------------------------------------------------------------
 *  Results container
 * ------------------------------------------------------------ */
typedef struct {
    double rt_mean;
    double rt_sd;
    double rt_cv;
    int rt_valid;
    int rt_lapses;
    int rt_premature;
    int spatial_correct;
    int span_correct;
    int search_correct;
    double search_time_avg;
} Results;
 
/* ------------------------------------------------------------
 *  TASK 1: Reaction consistency
 * ------------------------------------------------------------ */
static void run_reaction(Results *r)
{
    double rts[RT_TRIALS];
    int valid = 0;
    int lapses = 0;
    int premature = 0;
    int trial = 0;
    int attempts = 0;
    int i;
    double sum = 0.0;
    double sq = 0.0;
 
    clear_screen();
    printf("==============================================\n");
    printf("  TASK 1 of 4 : REACTION TEST\n");
    printf("==============================================\n\n");
    printf("A message will say 'GO!' after a random wait.\n");
    printf("Press the SPACEBAR as fast as you can when you see it.\n");
    printf("Do NOT press anything before 'GO!' appears.\n\n");
    printf("There are %d trials. Press ENTER to begin...", RT_TRIALS);
    fflush(stdout);
    wait_enter();
 
    while (trial < RT_TRIALS && attempts < RT_TRIALS * 3) {
        unsigned long delay_ms;
        unsigned long t0;
        unsigned long start;
        unsigned long t = 0;
        int early = 0;
        int hit = 0;
 
        attempts++;
        clear_screen();
        printf("Trial %d of %d\n\n", trial + 1, RT_TRIALS);
        printf("   Get ready... wait for the signal.\n");
        printf("   (do not press yet)\n");
        fflush(stdout);
        flush_keys();
 
        delay_ms = (unsigned long)rand_range(1500, 4000);
        t0 = now_ms();
        while (now_ms() - t0 < delay_ms) {
            if (key_pressed()) {
                get_key();
                early = 1;
                break;
            }
            sleep_ms(5);
        }
 
        if (early) {
            premature++;
            printf("\n   Too early! Wait for 'GO!'.\n");
            fflush(stdout);
            sleep_ms(1300);
            continue;
        }
 
        clear_screen();
        printf("\n\n\n        *****  GO!  PRESS SPACE  *****\n");
        fflush(stdout);
 
        start = now_ms();
        while (now_ms() - start < 3000UL) {
            if (key_pressed()) {
                get_key();
                t = now_ms() - start;
                hit = 1;
                break;
            }
            sleep_ms(1);
        }
 
        if (hit) {
            rts[valid++] = (double)t;
            if (t > 1000UL) {
                lapses++;
            }
            printf("\n        Reaction time: %lu ms\n", t);
        } else {
            rts[valid++] = 3000.0;
            lapses++;
            printf("\n        Missed! (no response)\n");
        }
        fflush(stdout);
        trial++;
        sleep_ms(900);
    }
 
    if (valid == 0) {
        r->rt_mean = 3000.0;
        r->rt_sd = 0.0;
        r->rt_cv = 0.0;
    } else {
        for (i = 0; i < valid; i++) {
            sum += rts[i];
        }
        r->rt_mean = sum / valid;
        for (i = 0; i < valid; i++) {
            sq += (rts[i] - r->rt_mean) * (rts[i] - r->rt_mean);
        }
        r->rt_sd = sqrt(sq / valid);
        r->rt_cv = r->rt_sd / r->rt_mean;
    }
    r->rt_valid = valid;
    r->rt_lapses = lapses;
    r->rt_premature = premature;
}
 
/* ------------------------------------------------------------
 *  TASK 2: Spatial pattern matching (mental rotation)
 * ------------------------------------------------------------ */
static void rotate_cw(int src[N][N], int dst[N][N])
{
    int i, j;
    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            dst[i][j] = src[N - 1 - j][i];
        }
    }
}
 
static void mirror_h(int src[N][N], int dst[N][N])
{
    int i, j;
    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            dst[i][j] = src[i][N - 1 - j];
        }
    }
}
 
static int grids_equal(int a[N][N], int b[N][N])
{
    int i, j;
    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            if (a[i][j] != b[i][j]) {
                return 0;
            }
        }
    }
    return 1;
}
 
static void copy_grid(int src[N][N], int dst[N][N])
{
    memcpy(dst, src, sizeof(int) * N * N);
}
 
static void print_grid_row(int g[N][N], int row)
{
    int j;
    for (j = 0; j < N; j++) {
        fputs(g[row][j] ? "[#]" : "[ ]", stdout);
    }
}
 
static int run_spatial(void)
{
    int correct = 0;
    int round;
 
    clear_screen();
    printf("==============================================\n");
    printf("  TASK 2 of 4 : SPATIAL PATTERN MATCHING\n");
    printf("==============================================\n\n");
    printf("You will see an ORIGINAL pattern of squares and three options.\n");
    printf("Pick the option that shows the ORIGINAL turned a quarter-turn\n");
    printf("CLOCKWISE (like a clock hand moving from 12 to 3).\n");
    printf("Other options are mirrored or slightly changed.\n\n");
    printf("Answer with the keys A, B or C.  %d rounds.\n\n", SPATIAL_ROUNDS);
    printf("Press ENTER to begin...");
    fflush(stdout);
    wait_enter();
 
    for (round = 1; round <= SPATIAL_ROUNDS; round++) {
        int orig[N][N];
        int rot[N][N];
        int d1[N][N];
        int d2[N][N];
        int opt[3][N][N];
        int i, j, cnt, pos, slot, c, choice;
 
        for (;;) {
            cnt = 0;
            for (i = 0; i < N; i++) {
                for (j = 0; j < N; j++) {
                    orig[i][j] = (rand() % 100) < 45 ? 1 : 0;
                    cnt += orig[i][j];
                }
            }
            if (cnt < 5 || cnt > 10) {
                continue;
            }
            rotate_cw(orig, rot);
            mirror_h(rot, d1);
            copy_grid(rot, d2);
            i = rand() % N;
            j = rand() % N;
            d2[i][j] = !d2[i][j];
            if (grids_equal(rot, d1) || grids_equal(d1, d2) ||
                grids_equal(rot, orig) || grids_equal(d1, orig) ||
                grids_equal(d2, orig)) {
                continue;
            }
            break;
        }
 
        pos = rand() % 3;
        slot = 0;
        for (i = 0; i < 3; i++) {
            if (i == pos) {
                copy_grid(rot, opt[i]);
            } else {
                copy_grid(slot == 0 ? d1 : d2, opt[i]);
                slot++;
            }
        }
 
        clear_screen();
        printf("Round %d of %d     Which option is the ORIGINAL turned CLOCKWISE?\n\n",
               round, SPATIAL_ROUNDS);
        printf("   ORIGINAL            A             B             C\n\n");
        for (i = 0; i < N; i++) {
            printf("   ");
            print_grid_row(orig, i);
            printf("      ");
            for (j = 0; j < 3; j++) {
                print_grid_row(opt[j], i);
                printf("     ");
            }
            printf("\n");
        }
        printf("\nYour answer (A/B/C): ");
        fflush(stdout);
 
        for (;;) {
            c = get_key();
            c = toupper(c);
            if (c == 'A' || c == 'B' || c == 'C') {
                break;
            }
        }
        putchar(c);
        choice = c - 'A';
        if (choice == pos) {
            correct++;
            printf("\n\nCorrect!\n");
        } else {
            printf("\n\nNot quite. The right answer was %c.\n", 'A' + pos);
        }
        fflush(stdout);
        sleep_ms(1200);
    }
    return correct;
}
 
/* ------------------------------------------------------------
 *  TASK 3: Backward digit span (working memory)
 * ------------------------------------------------------------ */
static int run_span(void)
{
    int lens[SPAN_ROUNDS];
    int correct = 0;
    int k;
 
    lens[0] = 3;
    lens[1] = 4;
    lens[2] = 5;
    lens[3] = 6;
 
    clear_screen();
    printf("==============================================\n");
    printf("  TASK 3 of 4 : BACKWARD DIGIT MEMORY\n");
    printf("==============================================\n\n");
    printf("Digits will flash on screen ONE AT A TIME.\n");
    printf("Afterwards type them in REVERSE order.\n\n");
    printf("Example: you see  1 ... 2 ... 3   then type  321\n\n");
    printf("%d rounds with increasing length.\n\n", SPAN_ROUNDS);
    printf("Press ENTER to begin...");
    fflush(stdout);
    wait_enter();
 
    for (k = 0; k < SPAN_ROUNDS; k++) {
        int digits[8];
        char expected[16];
        char answer[16];
        char buf[64];
        int len = lens[k];
        int i, a;
 
        clear_screen();
        printf("Round %d of %d : %d digits.\n\nPress ENTER when ready...",
               k + 1, SPAN_ROUNDS, len);
        fflush(stdout);
        wait_enter();
 
        for (i = 0; i < len; i++) {
            digits[i] = rand_range(0, 9);
            clear_screen();
            printf("\n\n\n\n               %d\n", digits[i]);
            fflush(stdout);
            sleep_ms(900);
            clear_screen();
            sleep_ms(350);
        }
 
        for (i = 0; i < len; i++) {
            expected[i] = (char)('0' + digits[len - 1 - i]);
        }
        expected[len] = '\0';
 
        flush_keys();
        clear_screen();
        printf("Type the digits in REVERSE order (last digit first),\n");
        printf("then press ENTER:\n\n> ");
        fflush(stdout);
        read_line(buf, (int)sizeof(buf));
 
        a = 0;
        for (i = 0; buf[i] != '\0' && a < 15; i++) {
            if (isdigit((unsigned char)buf[i])) {
                answer[a++] = buf[i];
            }
        }
        answer[a] = '\0';
 
        if (strcmp(answer, expected) == 0) {
            correct++;
            printf("\nCorrect!\n");
        } else {
            printf("\nNot quite. Correct answer was: %s\n", expected);
        }
        fflush(stdout);
        sleep_ms(1500);
    }
    return correct;
}
 
/* ------------------------------------------------------------
 *  TASK 4: Visual search
 * ------------------------------------------------------------ */
static int run_search(double *avg_time)
{
    int correct = 0;
    double total_time = 0.0;
    int round;
 
    clear_screen();
    printf("==============================================\n");
    printf("  TASK 4 of 4 : VISUAL SEARCH\n");
    printf("==============================================\n\n");
    printf("A grid of the letter 'O' will appear with ONE letter 'Q' hidden in it.\n");
    printf("Find the 'Q' and type its ROW number and COLUMN letter,\n");
    printf("for example:  3H   then press ENTER.\n\n");
    printf("You have %lu seconds per round.  %d rounds.\n\n",
           SEARCH_TIME_LIMIT_MS / 1000UL, SEARCH_ROUNDS);
    printf("Press ENTER to begin...");
    fflush(stdout);
    wait_enter();
 
    for (round = 1; round <= SEARCH_ROUNDS; round++) {
        int tr = rand() % SEARCH_ROWS;
        int tc = rand() % SEARCH_COLS;
        int i, j;
        unsigned long start, elapsed;
        char buf[32];
        int arow = -1;
        int acol = -1;
 
        clear_screen();
        printf("Round %d of %d  - find the 'Q'\n\n", round, SEARCH_ROUNDS);
        printf("     ");
        for (j = 0; j < SEARCH_COLS; j++) {
            printf("%c ", 'A' + j);
        }
        printf("\n");
        for (i = 0; i < SEARCH_ROWS; i++) {
            printf("  %d  ", i + 1);
            for (j = 0; j < SEARCH_COLS; j++) {
                printf("%c ", (i == tr && j == tc) ? 'Q' : 'O');
            }
            printf("\n");
        }
        printf("\nAnswer (row number + column letter, e.g. 3H): ");
        fflush(stdout);
 
        flush_keys();
        start = now_ms();
        read_line(buf, (int)sizeof(buf));
        elapsed = now_ms() - start;
 
        for (i = 0; buf[i] != '\0'; i++) {
            if (isdigit((unsigned char)buf[i]) && arow < 0) {
                arow = buf[i] - '1';
            } else if (isalpha((unsigned char)buf[i]) && acol < 0) {
                acol = toupper((unsigned char)buf[i]) - 'A';
            }
        }
 
        total_time += (double)elapsed / 1000.0;
 
        if (arow == tr && acol == tc && elapsed <= SEARCH_TIME_LIMIT_MS) {
            correct++;
            printf("\nCorrect!  (%.1f seconds)\n", (double)elapsed / 1000.0);
        } else if (arow == tr && acol == tc) {
            printf("\nRight spot, but too slow (%.1f seconds).\n",
                   (double)elapsed / 1000.0);
        } else {
            printf("\nNot quite. The 'Q' was at %d%c.\n", tr + 1, 'A' + tc);
        }
        fflush(stdout);
        sleep_ms(1500);
    }
    *avg_time = total_time / SEARCH_ROUNDS;
    return correct;
}
 
/* ------------------------------------------------------------
 *  Scoring and report
 * ------------------------------------------------------------ */
static void print_bar(double pts, double max_pts)
{
    int filled;
    int i;
    double perf = (max_pts - pts) / max_pts;
    if (perf < 0.0) {
        perf = 0.0;
    }
    if (perf > 1.0) {
        perf = 1.0;
    }
    filled = (int)(perf * 20.0 + 0.5);
    printf("[");
    for (i = 0; i < 20; i++) {
        putchar(i < filled ? '#' : '-');
    }
    printf("] %3d%%", (int)(perf * 100.0 + 0.5));
}
 
static void show_report(Results *r)
{
    double att_pts = 0.0;
    double spatial_pts;
    double mem_pts;
    double search_pts;
    double total;
    int fluctuation;
    int visuo_problem;
    int memory_ok;
 
    /* Attention / reaction consistency impairment points (max 25) */
    if (r->rt_mean <= 350.0) {
        att_pts += 0.0;
    } else if (r->rt_mean <= 500.0) {
        att_pts += 3.0;
    } else if (r->rt_mean <= 700.0) {
        att_pts += 6.0;
    } else {
        att_pts += 10.0;
    }
 
    if (r->rt_cv <= 0.20) {
        att_pts += 0.0;
    } else if (r->rt_cv <= 0.35) {
        att_pts += 3.0;
    } else if (r->rt_cv <= 0.50) {
        att_pts += 6.0;
    } else {
        att_pts += 10.0;
    }
 
    {
        double lapse_part = 2.0 * (r->rt_lapses + r->rt_premature);
        if (lapse_part > 5.0) {
            lapse_part = 5.0;
        }
        att_pts += lapse_part;
    }
 
    spatial_pts = (double)(SPATIAL_ROUNDS - r->spatial_correct) * 5.0;
    mem_pts = (double)(SPAN_ROUNDS - r->span_correct) * 6.25;
    search_pts = (double)(SEARCH_ROUNDS - r->search_correct) * 6.25;
    total = att_pts + spatial_pts + mem_pts + search_pts;
 
    fluctuation = (r->rt_cv > 0.35) || (r->rt_lapses >= 2);
    visuo_problem = (spatial_pts + search_pts) >= 15.0;
    memory_ok = mem_pts <= 6.25;
 
    clear_screen();
    printf("==============================================\n");
    printf("        SCREENING RESULTS\n");
    printf("==============================================\n\n");
 
    printf("1) Attention / Reaction Consistency\n");
    printf("   Mean reaction : %.0f ms\n", r->rt_mean);
    printf("   Variability   : %.0f ms  (consistency ratio %.2f)\n",
           r->rt_sd, r->rt_cv);
    printf("   Lapses (slow/missed): %d    Early presses: %d\n",
           r->rt_lapses, r->rt_premature);
    printf("   Performance   : ");
    print_bar(att_pts, 25.0);
    printf("\n\n");
 
    printf("2) Visual-Spatial Processing\n");
    printf("   Correct       : %d / %d\n", r->spatial_correct, SPATIAL_ROUNDS);
    printf("   Performance   : ");
    print_bar(spatial_pts, 25.0);
    printf("\n\n");
 
    printf("3) Working Memory (backward digits)\n");
    printf("   Correct       : %d / %d\n", r->span_correct, SPAN_ROUNDS);
    printf("   Performance   : ");
    print_bar(mem_pts, 25.0);
    printf("\n\n");
 
    printf("4) Visual Search / Scanning\n");
    printf("   Correct       : %d / %d   (avg %.1f s)\n",
           r->search_correct, SEARCH_ROUNDS, r->search_time_avg);
    printf("   Performance   : ");
    print_bar(search_pts, 25.0);
    printf("\n\n");
 
    printf("----------------------------------------------\n");
    printf("Overall difficulty score: %.1f / 100  (lower is better)\n", total);
    if (total < 20.0) {
        printf("Result: LOW indication of cognitive difficulty.\n");
    } else if (total < 45.0) {
        printf("Result: MODERATE indication - some difficulty noticed.\n");
    } else {
        printf("Result: HIGH indication - notable difficulty noticed.\n");
    }
 
    printf("\nPattern analysis:\n");
    if (fluctuation && visuo_problem) {
        printf(" * Fluctuating attention AND visual-spatial difficulty were seen.\n");
        printf("   This combination is one pattern associated with Lewy Body\n");
        printf("   Dementia, so a check-up with a doctor is a sensible next step.\n");
        if (memory_ok) {
            printf(" * Working memory was relatively preserved, which also fits\n");
            printf("   the typical early profile described for LBD.\n");
        }
    } else if (fluctuation) {
        printf(" * Reaction times varied a lot from trial to trial.\n");
        printf("   Fluctuating attention can have many causes (tiredness,\n");
        printf("   distraction, stress, sleep problems, medication).\n");
    } else if (visuo_problem) {
        printf(" * Visual-spatial tasks were harder than expected.\n");
        printf("   Eyesight, unfamiliarity with such puzzles or fatigue can\n");
        printf("   also affect this.\n");
    } else {
        printf(" * No strong LBD-like pattern (fluctuating attention combined\n");
        printf("   with visual-spatial difficulty) was detected.\n");
    }
 
    printf("\n==============================================\n");
    printf(" DISCLAIMER: This game is a SCREENING toy for education.\n");
    printf(" It cannot diagnose Lewy Body Dementia. Diagnosis needs a\n");
    printf(" clinical evaluation by a neurologist (including history of\n");
    printf(" visual hallucinations, REM sleep behaviour changes,\n");
    printf(" parkinsonism and fluctuating alertness, plus brain tests).\n");
    printf(" If you or a loved one are worried, please see a doctor.\n");
    printf("==============================================\n");
    fflush(stdout);
}
 
/* ------------------------------------------------------------
 *  Main
 * ------------------------------------------------------------ */
int main(void)
{
    char again[8];
 
    init_terminal();
    srand((unsigned int)time(NULL));
 
    do {
        Results res;
 
        memset(&res, 0, sizeof(res));
 
        clear_screen();
        printf("==============================================\n");
        printf("   LEWY BODY DEMENTIA - COGNITIVE SCREENING GAME\n");
        printf("==============================================\n\n");
        printf("This game checks four areas often affected in LBD:\n");
        printf("  1. Attention and reaction consistency\n");
        printf("  2. Visual-spatial processing\n");
        printf("  3. Working memory\n");
        printf("  4. Visual search\n\n");
        printf("It takes about 5-8 minutes. Sit comfortably, use your\n");
        printf("usual glasses, and try to do your best.\n\n");
        printf("NOTE: This is a screening game, NOT a medical diagnosis.\n\n");
        printf("Press ENTER to start...");
        fflush(stdout);
        wait_enter();
 
        run_reaction(&res);
        res.spatial_correct = run_spatial();
        res.span_correct = run_span();
        res.search_correct = run_search(&res.search_time_avg);
 
        show_report(&res);
 
        printf("\nPlay again? (Y/N): ");
        fflush(stdout);
        read_line(again, (int)sizeof(again));
    } while (again[0] == 'y' || again[0] == 'Y');
 
    clear_screen();
    printf("Thank you for playing. Take care!\n");
    return 0;
}
 
