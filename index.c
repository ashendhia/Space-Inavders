// ldd: -lncurses -lm -Wall -Wextra -pedantic -std=c11 -O3 -ffast-math -fno-omit-frame-pointer -frounding-math -fsignaling-nans
#include <ncurses.h>
#include <time.h>
#include <stdlib.h>
#include <fenv.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>

#define COLOR_ALIENS COLOR_PAIR(1)
#define COLOR_SHIELDS COLOR_PAIR(2)
#define COLOR_SHIP COLOR_PAIR(3)
#define COLOR_BOMBS COLOR_PAIR(4)
#define COLOR_SHOTS COLOR_PAIR(5)
#define COLOR_DARK COLOR_PAIR(6)
#define COLOR_LIGHT COLOR_PAIR(7)

#define SECOND 1000000
int shots_rate = 10;
int main_rate = 100;
int aliens_rate = 8;
int bombs_rate = 2;
int bombs_chance = 100;
int LINES, COLS, score = 0;
bool running = true;
pthread_mutex_t mutex, shields_lock, shots_lock, aliens_lock, bombs_lock, boss_lock;

int ship, health = 5; // Variable globale pour stocker la position du vaisseau
void init_ship()
{
    ship = floor((COLS - 2) / 2); // Initialiser la position du vaisseau
}

void print_ship()
{
    // Afficher le vaisseau à l'écran
    attron(COLOR_SHIP);
    move(LINES - 2, ship);
    addch(ACS_BLOCK);
    move(LINES - 1, ship - 1);
    addch(ACS_BLOCK);
    move(LINES - 1, ship);
    addch(ACS_BLOCK);
    move(LINES - 1, ship + 1);
    addch(ACS_BLOCK);
}

int **bombs; // Variable globale pour stocker le tableau des bombes
void init_bombs()
{
    bombs = (int **)malloc(LINES * sizeof(int *)); // Allouer de la mémoire pour le tableau des bombes
    for (int i = 0; i < LINES; i++)
    {
        bombs[i] = (int *)calloc(COLS, sizeof(int));
    }
}

void print_bombs()
{
    // Afficher les bombes à l'écran
    for (int i = 0; i < LINES; i++)
    {
        for (int j = 0; j < COLS; j++)
        {
            if (bombs[i][j] == 1)
            {
                move(i, j);
                attron(COLOR_BOMBS);
                addch(ACS_DIAMOND);
            }
        }
    }
}

int **shots; // Variable globale pour stocker le tableau des tirs
void init_shots()
{
    shots = (int **)malloc(LINES * sizeof(int *)); // Allouer de la mémoire pour le tableau des tirs
    for (int i = 0; i < LINES; i++)
    {
        shots[i] = (int *)calloc(COLS, sizeof(int));
    }
}

void print_shots()
{
    // Afficher les tirs à l'écran
    pthread_mutex_lock(&shots_lock);
    for (int i = 0; i < LINES; i++)
    {
        for (int j = 0; j < COLS; j++)
        {
            if (shots[i][j] == 1)
            {
                move(i, j);
                attron(COLOR_SHOTS);
                addch(ACS_DEGREE);
            }
        }
    }
    pthread_mutex_unlock(&shots_lock);
}

int **shields; // Variable globale pour stocker le tableau des boucliers
int init_shields(int count, int width)
{
    int space = floor((COLS - count * width) / (count + 1)); // Calculer l'espace entre les boucliers
    shields = (int **)malloc(2 * sizeof(int *));             // Allouer de la mémoire pour le tableau des boucliers
    for (int i = 0; i < 2; i++)
    {
        shields[i] = (int *)calloc(COLS, sizeof(int));
    }
    for (int i = 0; i < 2; i++)
    {

        int offset = space; // Calculer le décalage des boucliers
        for (int j = 0; j < count; j++)
        {
            for (int k = 0; k < width; k++)
            {
                shields[i][offset + k] = 1;
            }
            offset += space + width;
        }
    }
}

void print_shields()
{
    // Afficher les boucliers à l'écran
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < COLS; j++)
        {
            pthread_mutex_lock(&shields_lock);
            if (shields[i][j] == 1)
            {
                move(i + LINES - 5, j);
                attron(COLOR_SHIELDS);
                addch(ACS_BLOCK);
            }
            pthread_mutex_unlock(&shields_lock);
        }
    }
}

int **aliens, aliens_count, flow1 = 1, flow2 = -1;
int *boss, bossFlow = 1;
char **frames;
char *frames_data, *boss_frame;
int frames_count, frames_height, frames_width;

void init_aliens(char *path)
{
    // Open the file containing the frames
    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        fprintf(stderr, "Error opening file: %s\n", path);
        exit(1);
    }

    // Count the number of frames
    frames_count = 1;
    char c;
    int length = 0;
    int height = 0;

    while ((c = fgetc(file)) != EOF)
    {
        // if line is empty then increment frames_count
        if (c == '\n')
        {
            if (length == 0)
            {
                frames_count++;
                height = 0;
            }
            length = 0;
            height++;
        }
        else
        {
            length++;
            if (length > frames_width)
            {
                frames_width = length;
            }
        }
    }
    frames_height = height;
    rewind(file);

    frames = malloc(frames_count * sizeof(char *));

    // frames_data needs to be 2D array each row is a frame
    frames_data = malloc(frames_count * frames_width * frames_height * sizeof(char));

    // Read frames from the file
    length = 0;
    int k = 0;
    int i, j;

    for (i = 0; i < frames_count; i++)
    {
        frames[i] = &frames_data[k];
    restart_loop:
        length = 0;
        for (j = 0; j < frames_width + 1; j++)
        {
            c = fgetc(file);
            if (c != '\n')
            {
                length++;
                frames_data[k] = c;
                k++;
            }
            else if (c == '\n')
            {
                if (length != 0)
                {
                    goto restart_loop;
                }
                else
                {
                    while (k % (frames_width * frames_height) != 0)
                    {
                        frames_data[k] = '0';
                        k++;
                    }
                    break;
                }
            }
            else if (c == EOF)
            {
                break;
            }
        }
    }

    // Close the file
    fclose(file);

    // Initialize aliens array
    aliens_count = 12;
    int row = 0;
    int col = 0;
    // evenly space between aliens

    int space = floor((COLS - 6 - (aliens_count / 2) * frames_width) / (aliens_count / 2));

    aliens = malloc(aliens_count * sizeof(int *));
    for (i = 0; i < aliens_count; i++)
    {
        aliens[i] = malloc(3 * sizeof(int));
        aliens[i][0] = row;              // Position initiale de ligne
        aliens[i][1] = col;              // Position intiale de colonnes
        aliens[i][2] = i % frames_count; // Frame de l'alien choisir entre deux
        col = col + frames_width + space;
        if (i == floor(aliens_count / 2) - 1)
        {
            row = frames_height + 4;
            col = space;
        }
    }
}

void print_alien_frame(int frame, int row, int col)
{
    int i, j;
    for (i = 0; i < frames_height; i++)
    {
        for (j = 0; j < frames_width; j++)
        {
            move(row + i, col + j);
            if (frame == -1)
            {
                continue;
            }
            char c = frames[frame][i * frames_width + j];
            if (c == '#')
            {
                // Afficher un carré
                attron(COLOR_ALIENS);
                addch(ACS_BLOCK);
            }
            else if (c == ' ')
            {
                // Afficher un espace
                attron(COLOR_DARK);
                addch(' ');
            }
        }
    }
}

void print_aliens()
{
    int i;
    for (i = 0; i < aliens_count; i++)
    {
        int row = aliens[i][0];
        int col = aliens[i][1];
        int frame = aliens[i][2];
        print_alien_frame(frame, row, col);
    }
}

init_boss(char *path)
{

    // Open the file containing the frames
    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        fprintf(stderr, "Error opening file: %s\n", path);
        exit(1);
    }

    // Count the number of frames
    boss_frame = malloc(16 * 7 * sizeof(char));

    char c;
    int k = 0;

    while ((c = fgetc(file)) != EOF)
    {
        // if line is empty then increment frames_count
        if (c != '\n')
        {
            boss_frame[k] = c;
            k++;
        }
    }

    boss = malloc(3 * sizeof(int));
    boss[0] = 0;
    boss[1] = floor((COLS / 2) - 8);
    boss[2] = 30;

    // Close the file
    fclose(file);
}

void print_boss()
{
    for (int i = 0; i < 7; i++)
    {
        for (int j = 0; j < 16; j++)
        {
            move(boss[0] + i, boss[1] + j);
            char c = boss_frame[i * 16 + j];
            if (c == '#')
            {
                // Afficher un carré
                attron(COLOR_ALIENS);
                addch(ACS_BLOCK);
            }
            else if (c == ' ')
            {
                // Afficher un espace
                attron(COLOR_DARK);
                addch(' ');
            }
        }
    }
}

void init_game(char *aliens_path, int shields_count, int shields_width)
{
    init_ship();
    init_bombs();
    init_shots();
    init_shields(shields_count, shields_width);
    init_aliens(aliens_path);
    init_boss("boss.txt");
}

void print_game()
{
    erase(); // Effacer l'écran

    print_ship();
    print_bombs();
    print_shots();
    print_shields();
    if (aliens_count == 0)
        print_boss();
    else
        print_aliens();

    // Afficher le score
    move(0, COLS - 10);
    attron(COLOR_LIGHT);
    printw("Score: %d", score);

    // Afficher le nombre de vies
    move(0, 0);
    attron(COLOR_LIGHT);
    printw("Health: ");
    attron(COLOR_GREEN);
    for (int i = 0; i < health; i++)
    {
        addch(ACS_DIAMOND);
    }

    refresh(); // Rafraîchir l'écran
}

void INIT()
{
    initscr();                                // Initialise ncurses
    getmaxyx(stdscr, LINES, COLS);            // Stocker les dimensions de l'écran
    curs_set(0);                              // Masquer le curseur
    start_color();                            // Activer les couleurs
    init_pair(1, COLOR_BLUE, COLOR_BLUE);     // Definir la paire de couleurs pour les aliens
    init_pair(2, COLOR_YELLOW, COLOR_YELLOW); // Definir la paire de couleurs pour les boucliers
    init_pair(3, COLOR_GREEN, COLOR_GREEN);   // Definir la paire de couleurs pour le vaisseau
    init_pair(4, COLOR_RED, COLOR_BLACK);     // Definir la paire de couleurs pour les bombes
    init_pair(5, COLOR_WHITE, COLOR_BLACK);   // Definir la paire de couleurs pour les tirs
    init_pair(6, COLOR_BLACK, COLOR_BLACK);   // Definir la paire de couleurs pour le fond
    init_pair(7, COLOR_WHITE, COLOR_BLACK);   // Definir la paire de couleurs pour le texte
    raw();                                    // Désactiver le buffer de ligne
    keypad(stdscr, TRUE);                     // Activer les touches spéciales
    noecho();                                 // Désactiver l'affichage des touches
    nodelay(stdscr, TRUE);                    // Désactiver le blocage de getch()
    srand(time(NULL));                        // Initialiser le générateur de nombres aléatoires
    // Afficher le message de bienvenue
    attron(COLOR_LIGHT);
    mvprintw(LINES / 2 - 1, COLS / 2 - 15, "Bienvenue dans Space Invaders");
    mvprintw(LINES / 2, COLS / 2 - 19, "Appuyez sur une touche pour commencer");
    while (getch() == ERR)
        ;    // Attendre une touche
    clear(); // Effacer l'écran
    // countdown avant le début du jeu (3, 2, 1, GO!) chaque nombre est affiché pendant 1 seconde
    mvprintw(LINES / 2, COLS / 2 - 1, "3");
    refresh(); // Rafraîchir l'écran
    sleep(1);  // Attendre 1 seconde

    mvprintw(LINES / 2, COLS / 2 - 1, "2");
    refresh(); // Rafraîchir l'écran
    sleep(1);  // Attendre 1 seconde

    mvprintw(LINES / 2, COLS / 2 - 1, "1");
    refresh(); // Rafraîchir l'écran
    sleep(1);  // Attendre 1 seconde

    mvprintw(LINES / 2, COLS / 2 - 1, "GO!");
    refresh(); // Rafraîchir l'écran
    sleep(1);  // Attendre 1 seconde
    clear();   // Effacer l'écran

    attroff(COLOR_LIGHT);

    refresh();
}

void DONE()
{
    endwin(); // Terminer ncurses
}

void *shots_thread(void *arg)
{
    while (1)
    {
        usleep(SECOND / shots_rate);
        pthread_mutex_lock(&mutex);
        pthread_mutex_lock(&shots_lock);
        if (running)
        {
            for (int i = 0; i < LINES - 1; i++)
            {
                for (int j = 0; j < COLS; j++)
                {
                    shots[i][j] = shots[i + 1][j];
                }
            }
        }
        pthread_mutex_unlock(&shots_lock);
        pthread_mutex_unlock(&mutex);
    }
}

void *aliens_thread(void *arg)
{
    while (1)
    {
        usleep(SECOND / aliens_rate);
        pthread_mutex_lock(&mutex);
        pthread_mutex_lock(&aliens_lock);
        if (running)
        {
            if (aliens_count > 0)
            {
                int row = 0;
                int col = flow1;
                int row1 = aliens[0][0], first = 0, last = 0;
                int hw = floor(frames_width / 2);

                for (int i = 0; i < aliens_count; i++)
                {
                    if (aliens[i][0] == row1)
                    {
                        last = i;
                    }
                }
                if (aliens[first][1] == 0)
                {
                    flow1 = 1;
                    col = flow1;
                    row = 1;
                }
                if (aliens[last][1] == COLS - 6)
                {
                    flow1 = -1;
                    col = flow1;
                }
                for (int i = 0; i <= last; i++)
                {
                    aliens[i][0] += row;
                    aliens[i][1] += col;
                    // get random number to throw a bomb with a probability of 1/12
                    int chance = rand() % bombs_chance;
                    if (chance == 0)
                    {
                        pthread_mutex_lock(&bombs_lock);
                        bombs[aliens[i][0] + hw][aliens[i][1]] = 1;
                        pthread_mutex_unlock(&bombs_lock);
                    }
                }
                col = flow2;
                if (aliens[last + 1][1] == 0)
                {
                    flow2 = 1;
                    col = flow2;
                }
                if (aliens[aliens_count - 1][1] == COLS - 6)
                {
                    flow2 = -1;
                    col = flow2;
                }
                for (int i = last + 1; i < aliens_count; i++)
                {
                    aliens[i][0] += row;
                    aliens[i][1] += col;
                    // get random number to throw a bomb with a probability of 1/12
                    int chance = rand() % bombs_chance;
                    if (chance == 0)
                    {
                        pthread_mutex_lock(&bombs_lock);
                        bombs[aliens[i][0] + hw][aliens[i][1]] = 1;
                        pthread_mutex_unlock(&bombs_lock);
                    }
                }
            }
            else
            {
                // print boss
                pthread_mutex_lock(&boss_lock);
                aliens_rate = 16;
                if (boss[1] == COLS - 16)
                {
                    bossFlow = -1;
                }
                else if (boss[1] == 0)
                {
                    bossFlow = 1;
                    boss[0] += 2;
                }
                boss[1] += bossFlow;
                int chance = rand() % bombs_chance / 4;
                if (chance == 0)
                {
                    pthread_mutex_lock(&bombs_lock);
                    bombs[boss[0] + 8][boss[1]] = 1;
                    pthread_mutex_unlock(&bombs_lock);
                }
                pthread_mutex_unlock(&boss_lock);
            }
        }
        pthread_mutex_unlock(&aliens_lock);
        pthread_mutex_unlock(&mutex);
    }
}

void *bombs_thread(void *arg)
{
    while (1)
    {
        usleep(SECOND / bombs_rate);
        pthread_mutex_lock(&mutex);
        pthread_mutex_lock(&bombs_lock);
        if (running)
        {
            for (int i = LINES - 1; i > 0; i--)
            {
                for (int j = 0; j < COLS; j++)
                {
                    bombs[i][j] = bombs[i - 1][j];
                }
            }
        }
        pthread_mutex_unlock(&bombs_lock);
        pthread_mutex_unlock(&mutex);
    }
}

void *sound_thread(void *arg)
{
    while (1)
    {
        bool play = false;
        pthread_mutex_lock(&mutex);
        if (running)
        {
            play = true;
        }
        pthread_mutex_unlock(&mutex);
        // Execute SoX command
        if (play)
        {
            const char *playCommand = "play -q -t mp3 ./music/main.mp3 repeat 9999 trim 0 188";
            // Execute the play command
            system(playCommand);
        }
    }
}

void collision_detection()
{
    // Détection des collisions entres les tirs et les aliens et les boucliers et les bombes

    pthread_mutex_lock(&shots_lock);
    for (int i = 1; i < LINES; i++)
    {
        for (int j = 0; j < COLS; j++)
        {
            if (shots[i][j] == 1)
            {
                // vérifier si le tir a touché un bouclier
                if (i == (LINES - 4))
                {
                    pthread_mutex_lock(&shields_lock);
                    if (shields[1][j] == 1)
                    {
                        shots[i][j] = 0;
                    }
                    pthread_mutex_unlock(&shields_lock);
                }

                // vérifier si le tir a touché un alien
                pthread_mutex_lock(&aliens_lock);
                for (int a = 0; a < aliens_count; a++)
                {
                    if ((i >= (aliens[a][0]) && (i <= aliens[a][0] + frames_height)) && (j >= (aliens[a][1]) && (j <= aliens[a][1] + frames_width)))
                    {
                        for (int k = a; k < aliens_count - 1; k++)
                        {
                            aliens[k][0] = aliens[k + 1][0];
                            aliens[k][1] = aliens[k + 1][1];
                            aliens[k][2] = aliens[k + 1][2];
                        }
                        aliens_count--;
                        shots[i][j] = 0;
                        score += 10;
                    }
                }
                pthread_mutex_lock(&boss_lock);
                if (aliens_count == 0)
                {
                    if ((i >= (boss[0]) && (i <= boss[0] + frames_height)) && (j >= (boss[1]) && (j <= boss[1] + frames_width)))
                    {
                        boss[2] -= 1;
                        shots[i][j] = 0;
                        score += 5;
                        if (boss[2] == 0)
                        {
                            free(boss);
                            boss = NULL;
                            score += 100;
                            game_over(1);
                        }
                    }
                }
                pthread_mutex_unlock(&boss_lock);
                pthread_mutex_unlock(&aliens_lock);

                // vérifier si le tir a touché une bombe
                pthread_mutex_lock(&bombs_lock);
                if (bombs[i][j] == 1 || bombs[i - 1][j] == 1)
                {
                    if (bombs[i][j] == 1)
                    {
                        bombs[i][j] = 0;
                    }
                    else
                    {
                        bombs[i - 1][j] = 0;
                    }
                    shots[i][j] = 0;
                    score += 5;
                }
                pthread_mutex_unlock(&bombs_lock);
            }
        }
    }
    pthread_mutex_unlock(&shots_lock);

    // Détection des collisions entres les bombes et les boucliers et le vaisseau

    pthread_mutex_lock(&bombs_lock);
    for (int i = LINES - 1; i > 0; i--)
    {
        for (int j = 0; j < COLS; j++)
        {
            if (bombs[i][j] == 1)
            {
                // vérifier si la bombe a touché un bouclier
                if (i == (LINES - 4) || i == LINES - 5)
                {
                    pthread_mutex_lock(&shields_lock);
                    if (shields[0][j] == 1)
                    {
                        shields[0][j] = 0;
                        bombs[i][j] = 0;
                    }
                    else
                    {
                        if (shields[1][j] == 1)
                        {
                            shields[1][j] = 0;
                            bombs[i][j] = 0;
                        }
                    }
                    pthread_mutex_unlock(&shields_lock);
                }

                // vérifier si la bombe a touché le vaisseau
                if (i == (LINES - 2))
                {
                    if (j >= (ship - 1) && j <= (ship + 1))
                    {
                        bombs[i][j] = 0;
                        game_over(0);
                    }
                }
            }
        }
    }
    pthread_mutex_unlock(&bombs_lock);
}

void wait_key()
{
    int ch = getch();
    if (ch == KEY_BACKSPACE)
    {
        move(0, 0);
        attron(COLOR_LIGHT);
        printw("Backspace pressed");
        DONE();
        exit(0);
    }
    if (ch == KEY_LEFT)
    {
        if (ship > 1)
        {
            ship = ship - 1;
        }
    }
    if (ch == KEY_RIGHT)
    {
        if (ship < COLS - 3)
        {
            ship = ship + 1;
        }
    }
    if (ch == ' ')
    {
        pthread_mutex_lock(&shots_lock);
        shots[LINES - 3][ship] = 1;
        score -= 1;
        pthread_mutex_unlock(&shots_lock);
    }
    if (ch == 'p')
    {
        running = false;
        attron(COLOR_LIGHT);
        move(LINES / 2, COLS / 2 - 2);
        printw("Pause");
        while (getch() != 'p')
            ;
        clear();
        attroff(COLOR_LIGHT);
        running = true;
    }
}

void game_over(int won)
{
    // Afficher le message de fin de jeu
    attron(COLOR_LIGHT);
    if (won == 1)
    {
        mvprintw(LINES / 2, COLS / 2 - 3, "YOU WON");
    }
    else
    {
        mvprintw(LINES / 2, COLS / 2 - 4, "GAME OVER");
    }
    refresh();
    sleep(2);
    DONE();
    exit(0);
}

int main()
{

    feenableexcept(FE_DIVBYZERO | FE_INVALID | FE_OVERFLOW);
    INIT();
    // Initiliaser le jeu
    init_game("aliens.txt", 8, 5);

    // Créer les threads
    if (pthread_mutex_init(&mutex, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }
    if (pthread_mutex_init(&shots_lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }
    if (pthread_mutex_init(&shields_lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }
    if (pthread_mutex_init(&aliens_lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }
    if (pthread_mutex_init(&bombs_lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }

    pthread_t thread1, thread2, thread3, thread4;
    pthread_create(&thread1, NULL, shots_thread, NULL);
    pthread_create(&thread2, NULL, aliens_thread, NULL);
    pthread_create(&thread3, NULL, bombs_thread, NULL);
    pthread_create(&thread4, NULL, sound_thread, NULL);

    // Boucle principale du jeu
    while (1)
    {
        usleep(SECOND / main_rate);
        collision_detection();
        print_game();
        wait_key();
    }

    DONE();
    return 0;
}