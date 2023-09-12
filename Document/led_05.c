// g++cmd:g++ -o led_05 led_05.c -Wall -I /home/metoro/rpi-rgb-led-matrix/include -L /home/metoro/rpi-rgb-led-matrix/lib -lrgbmatrix -I /home/metoro/stb -g -O0 

#define STB_IMAGE_IMPLEMENTATION

#include <stdio.h>        //標準入出力
#include <stdlib.h>       //メモリ関係
#include <dirent.h>       //ディレクトリ関係
#include <string.h>       //文字列関係
#include <errno.h>        //エラー関係
#include <led-matrix-c.h> //パネル制御
#include <stb_image.h>    //画像を読み込む
#include <unistd.h>       //sleep()用

char **get_filename(char *, int *);                           // ディレクトリの中の画像を取得
void add_option(int *, char ***, int, char **, int, char **); // 設定用コマンドライン引数を生成
void print_canvas(int, int, int, int, unsigned char *);       // データをパネルに反映
void reset_canvas(int panel_x, int panel_y);                 // キャンバスをリセット
char *extract_option_name(char *, int *, char *);             // ファイル名に指定された指定された画像の名前を返す

struct LedCanvas *offscreen_canvas;  // キャンバス(ライブラリの仕様のためグローバル変数にしざるを得ない)
struct RGBLedMatrix *matrix_options; // 関数がパネルの設定を入れる構造体(同上)
struct RGBLedMatrixOptions options;  // 設定を入れる構造体(同上)

int main(int argc, char **argv)
{
    /*ユーザーが設定する変数*/

    char **argv_add = NULL;
    char argv_add_tmp[][256] = {"--led-slowdown-gpio=2", "--led-no-drop-privs", "--led-cols=64", "--led-rows=32", "--led-chain=3", "--led-pwm-bits=4","--led-show-refresh","--led-limit-refresh=120"}; // 補完するオプション

    char path[] = "/home/metoro/images"; // 画像を入れるフォルダ

    int wait_time = 5; // スライドの待機時間

    /*プログラムで使う変数*/
    int argc_copy = 0;                           // コマンドライン引数コピー用
    char **argv_copy = NULL;                     // コマンドライン引数コピー用
    char **file_list = NULL;                     // ファイル名一覧
    int file_num = 0;                            // ファイルの数
    int led_width = 0, led_height = 0;           // パネルの幅。高さ
    int image_x = 0, image_y = 0, image_bpp = 0; // 画像の幅・高さ
    char *buf = NULL;                            // ファイルパス関係の処理
    unsigned char *pixel = NULL;                 // 画像を格納
    int option_num = 0;                          // セットになった画像の数
    int option_progress = 0;                     // セットになった画像の読み込み具合
    char buf_copy[256];                          // 抽出したファイル名が入る

    buf = (char *)malloc(sizeof(char) * 256);
    if (buf == NULL)
    {
        exit(4);
    }

    memset(&options, 0, sizeof(options)); // 設定をデフォルトに

    argv_add = (char **)malloc(sizeof(argv_add_tmp) / sizeof(*argv_add_tmp) * sizeof(char *));
    for (int i = 0; i < (int)(sizeof(argv_add_tmp) / sizeof(*argv_add_tmp)); i++)
    {
        argv_add[i] = &argv_add_tmp[i][0];
    }

    /*初期設定*/

    add_option(&argc_copy, &argv_copy, argc, argv, sizeof(argv_add_tmp) / sizeof(*argv_add_tmp), argv_add); // コマンドライン引数を補完

    /*ファイルリストを取得*/

    file_list = get_filename(path, &file_num);

    /*パネルの初期設定*/

    matrix_options = led_matrix_create_from_options(&options, &argc_copy, &argv_copy); // 設定項目を反映させる
    if (matrix_options == NULL)
    {
        exit(1);
    }

    /*キャンバスの準備*/

    offscreen_canvas = led_matrix_create_offscreen_canvas(matrix_options); // キャンバスを生成
    led_canvas_get_size(offscreen_canvas, &led_width, &led_height);        // キャンバスの幅・高さを取得

    /*ここからループ*/
    while (1)
    {
        for (int i = 0; i < file_num; i++)
        {
            snprintf(buf, 257, "%s/%s", path, file_list[i]); // フルパスを生成

            pixel = stbi_load(buf, &image_x, &image_y, &image_bpp, 3); // 画像を読み込み

            if (pixel == NULL)
            {
                exit(10);
            }

            print_canvas(led_width - image_x, led_height - image_y, image_x, image_y, pixel); // データをキャンバスに反映
            offscreen_canvas = led_matrix_swap_on_vsync(matrix_options, offscreen_canvas);    // キャンバスをパネルに反映

            stbi_image_free(pixel); // メモリを解放
            sleep(wait_time);

            /*セットになった画像の処理*/

            option_num = 0;

            for (int j = 0; j < 256; j++) // 指定された画像の数をカウント
            {
                if (file_list[i][j] == '-')
                {
                    option_num++;
                }
            }

            option_progress = 0;

            for (int j = 0; j < option_num; j++)
            {
                snprintf(buf, 257, "%s/%s", path, extract_option_name(file_list[i], &option_progress, buf_copy)); // フルパスを生成

                pixel = stbi_load(buf, &image_x, &image_y, &image_bpp, 3); // 画像を読み込み

                if (pixel == NULL)
                {
                    exit(10);
                }

                print_canvas(led_width - image_x, led_height - image_y, image_x, image_y, pixel); // データをキャンバスに反映
                offscreen_canvas = led_matrix_swap_on_vsync(matrix_options, offscreen_canvas);    // キャンバスをパネルに反映

                stbi_image_free(pixel); // メモリを解放
                sleep(wait_time);
            }
        }
    }
}

char **get_filename(char *path, int *n) // ファイル名を取得
{
    /*変数の宣言*/
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    char **filename_list = NULL, **buf = NULL;
    char ext[] = ".png"; // 抜粋する拡張子

    dir = opendir(path); // ディレクトリを開く
    if (dir == NULL)     // エラーメッセージ集
    {
        if (errno == ENOENT)
        {
            printf("ディレクトリが存在しません。\n");
        }
        else if (errno == EACCES)
        {
            printf("ディレクトリにアクセスできません。\n");
        }
        else
        {
            printf("ディレクトリのオープンに失敗しました。エラーコード: %d\n", errno);
        }
        fflush(stdout);
        return NULL;
    }

    /*指定した拡張子のファイルを抜粋*/
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) //.を無視
        {
            continue;
        }

        if (strstr(entry->d_name, ext) == NULL) // 指定した拡張子を含まなかったらcontinue
        {
            continue;
        }

        if (strstr(entry->d_name, "_.") != NULL) // "_."を含む(他の幕とセット指定)ならcontinue
        {
            continue;
        }

        // 条件に合うものが見つかったらカウンタを+1して、パスを配列に格納
        *n = *n + 1;
        buf = (char **)realloc(filename_list, *n * sizeof(char *));
        if (buf == NULL)
        {
            exit(1);
        }
        filename_list = buf;
        buf = NULL;

        filename_list[(*n) - 1] = (char *)calloc(256, sizeof(char)); // readdirはNAME_MAX(255)+1文字を返してくる
        if ((filename_list[(*n) - 1]) == NULL)
        {
            exit(2);
        }

        strcpy(filename_list[(*n) - 1], entry->d_name);
    }

    return filename_list;
}
// コマンドライン引数を生成
void add_option(int *argc_copy, char ***argv_copy, int argc, char **argv, int argc_add, char **argv_add)
{
    int add_flug = 0;
    if (strcmp(argv[argc - 1], "-add") == 0) // 補完なしを指定された場合
    {
        add_flug = 1;
        argc_add = 0;
    }

    *argc_copy = argc - add_flug + argc_add; // オプション追加後のargcを計算
    *argv_copy = (char **)malloc(sizeof(char *) * (*argc_copy));
    if (*argv_copy == NULL)
    {
        exit(2);
    }

    for (int i = 0; i < argc - add_flug; i++)
    {
        (*argv_copy)[i] = argv[i];
        fflush(stdout);
    }

    for (int i = argc; i < *argc_copy; i++)
    {
        (*argv_copy)[i] = argv_add[i - argc - add_flug];
    }
}

void print_canvas(int start_x, int stary_y, int data_width, int data_height, unsigned char *pixel) // データを(パネルに表示パネル制御系の構造体をグローバルで宣言すること
{
    for (int y = 0; y < data_height; y++)
    {
        int n = 0;
        for (int x = 0; x < data_width; x++)
        {
            n = (y * data_width + x) * 3;
            led_canvas_set_pixel(offscreen_canvas, start_x + x, stary_y + y, pixel[n], pixel[n + 1], pixel[n + 2]); // キャンバスの座標x,yの色を設定
        }
    }
}

void reset_canvas(int panel_x, int panel_y)
{
    for (int y = 0; y < panel_y; y++)
    {
        for (int x = 0; x < panel_x; x++)
        {
            led_canvas_set_pixel(offscreen_canvas, panel_x, panel_y, 0, 0, 0); // キャンバスの座標x,yの色を(0,0,0)に設定
        }
    }
}

char *extract_option_name(char *file_name, int *option_progress, char *buf_copy_ext)
{
    int option_long = 0;
    char buf_copy[256];
    /*二枚目以降の表示*/

    for (*option_progress = *option_progress; 1; (*option_progress)++) // オプションが何文字目から始まるかチェック
    {
        if (file_name[*option_progress] == '-')
        {
            break;
        }
    }

    for (option_long = 1; 1; option_long++) // オプションの長さをチェック
    {
        if ((file_name[*option_progress + option_long] == '-') || (file_name[*option_progress + option_long] == '.'))
        {
            break;
        }
    }

    for (int k = 0; k < option_long - 1; k++) // オプションをコピー
    {
        buf_copy[k] = file_name[*option_progress + k + 1];
    }

    buf_copy[option_long - 1] = '\0';

    *option_progress = *option_progress + option_long; // オプションの読み込み具合を更新

    snprintf(buf_copy_ext, 261, "%s_.png", buf_copy); // 拡張子を追加

    return buf_copy_ext;
}
