/********************************************************************/
/*                                                                  */
/*                                                                  */
/*                                                                  */
/*                      c  p  m  a  d  d  8  8                      */
/*                                                                  */
/*                                                                  */
/*                               for                                */
/*            writing a data file into a CP/M disk image            */
/*                       in .d88 file format.                       */
/*                                                                  */
/*                            Ver.  0.1.3                           */
/*                                                                  */
/*                                                  (C) 2024 Tsugu  */
/*                                                                  */
/*               This software is released under the                */
/*                                                                  */
/*                           MIT License.                           */
/*        (https://opensource.org/licenses/mit-license.php)         */
/*                                                                  */
/********************************************************************/
//
// 動作のあらまし : .d88 形式の CP/M ディスクイメージ内末尾の空き領域に、
//                テキストファイルおよびバイナリデータを書き込む。
//                したがって、断片化した空き領域はすべて無視する。
//                ファイルは一つずつしか書き込めない。
//                同名ファイルがあったときは書き込みを行わない。
//                コマンドライン引数なしで普通に起動すると、使い方の簡単な説明が出る。
//
//
//
//               Strcture of FCB (File Control Block)
//
//
// addr    :  00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
// xxxx    : |DR|n0|n1|n2|n3|n4|n5|n6|n7|e0|e1|e2|LE|00|00|NR|
// xxxx+16 : |a0|a1|a2|a3|a4|a5|a6|a7|a8|a9|aA|aB|aC|aD|aE|aF|
//
// DR (drive number) :
//           00 = login disk, 01 = drive A, 02 = drive B, ...,
//           E5 = not on any drive.
// n0 ~ n7 (file name) :
//           末尾文字以降には値 20 (space) が詰められる。
// e0 ~ e2 (extention) :
//           末尾文字以降には値 20 が詰められる。
// LE (logical extend number) :
//           同名ファイルの FCB の内、16k バイト単位で何番目か。
//           0 番から始まる。
// NR (numbers of Records) : 使用しているレコード（セクタと同じサイズ）の数。
// a0 ~ aF (disk allocation map) :
//           使用している data block の番号をすべて列挙する。
//           末尾以降は値 00。
//           data block 0 番と 1 番は使用しない。
//
// data block のエリアは track 2 の先頭から始まる。
// data block 0 番と 1 番はディレクトリ・エリアで、FCB が書き込まれる。
// data block 2 番以降がデータ・レコード・エリアである。
// 注意点。
// 8 inch 標準・片面ディスクでは、1 data block = 1k (1024) bytes.
// 5 inch ミニ・両面ディスクでは、1 data block = 2k (2048) bytes.
// 消去したファイルの FCB の先頭には値 E5 が書き込まれる（すなわち DR = E5）。
//
// 8 inch 標準・片面ディスク : 128 bytes / 1 sector,
//                          26 sectors / 1 track,
//                          77 tracks / 1 disk.
// 5 inch 標準・両面ディスク : 256 bytes / 1 sector,
//                          32 sectors / 1 track,
//                          40 tracks / 1 disk.
// （ただし、このプログラムでは、8 inch のほうしか扱わない）
//
// テキストデータの場合、128 バイトごとに区切ったとき足りない部分が出ることがあるが、
// そこには 1A を埋める。
// 5 inch ミニ・両面ディスクにおいては、一つの FCB の真ん中で言わば切れている。
// すなわち、a7 までの ブロックが満杯 (=80H) になったら、NR は 0 にリセットされる。
//
//
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define DATA_BLOCK_SIZE 2048
#define BPS 256   // bytes per sector
#define SPT 32    // sectors per track
#define TRACKS 40 // number of all tracks
#define RPD 16    // records per data blocks

/* convert_addr(addr) */
// address on a disk --> address on a .d88 file
// PC-8801 のエミュレータ用の .d88 ファイルのデータは、
// 256 バイトごとに 16 バイト長のヘッダーのようなものがついている。
// 注意点として、全体の先頭にはさらに 43 行の .d88 専用領域が付いている。
int convert_addr(int addr)
{
  return 43 * 16 + 16 + addr + 16 * (addr / 256);
}

char *image_filename;
char *cpm_filename;
char FCB_buff[32];
char record_buff[128];
const int num_of_FCBs = DATA_BLOCK_SIZE * 2 / 32;                          // = 128
const int num_of_data_blocks = BPS * SPT * (TRACKS - 2) / DATA_BLOCK_SIZE; // = 152

char name[8 + 1] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '\0'}; // ' ' = 20
char extension[3 + 1] = {' ', ' ', ' ', '\0'};

void separate_name()
{
  char *ext = strrchr(cpm_filename, '.') + 1;
  int len, i;

  len = strlen(cpm_filename) - strlen(ext) - 1;
  for (i = 0; i < (len < 8 ? len : 8); i++)
    name[i] = toupper(cpm_filename[i]);

  len = strlen(ext);
  for (i = 0; i < (len < 3 ? len : 3); i++)
    extension[i] = toupper(ext[i]);
}

int head_addr_of_data_block(int block_number)
{
  return BPS * SPT * 2 + DATA_BLOCK_SIZE * block_number;
}

int head_addr_of_FCB(int FCB_number)
{
  return head_addr_of_data_block(0) + 32 * FCB_number;
}

int head_addr_of_record(int record_number)
{
  return head_addr_of_data_block(record_number / RPD) + 128 * (record_number % RPD);
}

int is_name_ext()
{
  int i;

  for (i = 1; i < 9; i++)
    if (FCB_buff[i] != name[i - 1])
      return 0;
  for (i = 9; i < 12; i++)
    if (FCB_buff[i] != extension[i - 9])
      return 0;

  return 1;
}

int last_living_block_num = 1; // 書き込んだ内容が生き残っている data block の最後の番号。
int last_living_FCB_num = -1;  // それに対応する FCB の番号。初期値 -1 は、存在しない場合。

/* same_name_check() */
// 同じ名前のファイルがすでにあるかチェック。もしあるなら書き込まない。
// last_living_FCB_num, last_living_block_num の更新も行う。
int same_name_check()
{
  FILE *fp = NULL;
  fp = fopen(image_filename, "rb");

  int r = 0;

  for (int i = 0; i < num_of_FCBs; i++)
  {
    fseek(fp, convert_addr(head_addr_of_FCB(i)), SEEK_SET);
    fread(FCB_buff, 1, 32, fp);

    if (FCB_buff[0] == 0) // living FCB
    {
      last_living_FCB_num = i;
      if (is_name_ext())
        r = 1;
      for (int j = 16; j < 32; j++)
        if (last_living_block_num < FCB_buff[j])
        {
          last_living_block_num = FCB_buff[j];
        }
    }
  }

  fclose(fp);
  return r;
}

void init_FCB_buf()
{
  int i;

  FCB_buff[0] = 0;
  for (i = 1; i < 9; i++)
    FCB_buff[i] = name[i - 1];
  for (i = 9; i < 12; i++)
    FCB_buff[i] = extension[i - 9];
  for (i = 12; i < 32; i++)
    FCB_buff[i] = 0;
}

void init_record_buf()
{
  for (int i = 0; i < 128; i++)
    record_buff[i] = 0x1A;
}

void save_FCB(FILE *fp, int FCB_number)
{
  fseek(fp, convert_addr(head_addr_of_FCB(FCB_number)), SEEK_SET);
  fwrite(FCB_buff, 1, 32, fp);
}

void save_record(FILE *fp, int record_number)
{
  fseek(fp, convert_addr(head_addr_of_record(record_number)), SEEK_SET);
  fwrite(record_buff, 1, 128, fp);
}

void write_in()
{
  separate_name();
  if (same_name_check())
  {
    printf("A same name file exists. Cancel writing.\n");
    return;
  }

  FILE *wfp = NULL;
  FILE *rfp = NULL;
  wfp = fopen(image_filename, "r+b"); // 部分的な上書きができないといけない。
  rfp = fopen(cpm_filename, "rb");

  int used_FCBs = 0;
  int written_records = 0;
  fseek(rfp, 0, SEEK_SET);
  int count = 0;
  init_FCB_buf();
  init_record_buf();
  while (fread(record_buff, 1, 128, rfp))
  {
    if (last_living_FCB_num + used_FCBs >= num_of_FCBs || RPD * (last_living_block_num + 1) + written_records >= RPD * num_of_data_blocks)
    {
      printf("Not enough capacity. The writing is incomplete.\n");
      return;
    }

    save_record(wfp, RPD * (last_living_block_num + 1) + written_records);
    written_records++;
    FCB_buff[12] = used_FCBs;
    FCB_buff[15] = written_records % 128 ? written_records % 128 : 0x80;
    int written_blocks = written_records / RPD + (written_records % RPD ? 1 : 0);
    int offset = (16 + written_blocks - 1) % 16;
    FCB_buff[16 + offset] = last_living_block_num + written_blocks;

    if (written_records % 128 == 0)
      used_FCBs++;
    if (written_records == BPS)
    {
      save_FCB(wfp, last_living_FCB_num + 1 + used_FCBs % 2);
      init_FCB_buf();
    }

    init_record_buf();
  }
  save_FCB(wfp, last_living_FCB_num + 1 + used_FCBs % 2);

  fclose(rfp);
  fclose(wfp);
  printf("Done.\n");
}

int main(int argc, char *argv[])
{
  switch (argc)
  {
  case 1:
    printf("Usage:\n");
    printf(" ./<this program name> <.d88 file name> <CP/M-80 file name> [RETURN]\n");
    printf(" It writes the file <CP/M-80 file name> into the file <.d88 file name> for PC-8801 emulators.\n");
    return 0;
  case 3:
    image_filename = argv[1];
    cpm_filename = argv[2];
    printf("%s --> %s\n", cpm_filename, image_filename);
    write_in();
    return 0;
  default:
    printf("Invalid arguments.\n");
    return 1;
  }
}
