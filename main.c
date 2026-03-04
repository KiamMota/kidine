#include <fcntl.h>
#include <stdio.h>

#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define WIDTH 80
#define SAVE_CURSOR() fputs("\033[s", stdout)
#define RESTORE_CURSOR() fputs("\033[u", stdout)

struct termios orig_term;

typedef enum {
  NORMAL,
  INSERT

} MODE_ENUM;

typedef enum { NONE, UP, DOWN, LEFT, RIGHT, ESCAPE } ACTION_ENUM;

typedef struct {
  MODE_ENUM _mode;
  ACTION_ENUM _action;
  char *_curr_buff;
} kidine_t;

kidine_t *init_editor(const char *_filename) {
  kidine_t *k = malloc(sizeof(kidine_t));
  k->_curr_buff = malloc(sizeof(strlen(_filename)));
  memcpy(k->_curr_buff, _filename, strlen(_filename));
  return k;
}

static kidine_t editor;
void kidine_change_action(kidine_t *k, ACTION_ENUM ac) { k->_action = ac; }
void kidine_change_mode(kidine_t *k, MODE_ENUM md) { k->_mode = md; }

const char *action_enum_to_str(ACTION_ENUM enm) {
  switch (enm) {
  case DOWN:
    return "move down";
  case UP:
    return "move up";
  case LEFT:
    return "move left";
  case RIGHT:
    return "move right";
  case NONE:
    return "none";
  case ESCAPE:
    return "escape";
  default:
    return "unk";
  }
}

const char *get_mode_str(kidine_t *k) {
  switch (k->_mode) {
  case NORMAL:
    return "NORMAL";
  case INSERT:
    return "INSERT";
  }
}

void render_info(kidine_t *k, int *view_offset) {
  RESTORE_CURSOR();
  printf("offset:       %d\n", *view_offset);
  printf("mode:         %s\n", get_mode_str(k));
  printf("action:       %s\n", action_enum_to_str(k->_action));
  printf("buffer name:  %s\n", k->_curr_buff);
  fflush(stdout);
}

long get_file_size(int fd) {
  off_t current = lseek(fd, 0, SEEK_CUR);
  if (current == (off_t)-1) {
    perror("lseek (SEEK_CUR)");
    return -1;
  }

  off_t size = lseek(fd, 0, SEEK_END);
  if (size == (off_t)-1) {
    perror("lseek (SEEK_END)");
    return -1;
  }

  // Volta para posição original
  if (lseek(fd, current, SEEK_SET) == (off_t)-1) {
    perror("lseek (restore)");
    return -1;
  }

  return (long)size;
}

void enable_raw_mode(struct termios *orig_term) {
  tcgetattr(STDIN_FILENO, orig_term);
  struct termios raw = *orig_term;
  raw.c_lflag &= ~(ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void render(char *content) {
  printf("\033[s"); // salva posição

  printf("[%-80.80s]\n", content);

  printf("\033[u"); // restaura posição
  fflush(stdout);
}
int build_view(char *buff, int offset, char *view_buffer) {
  int i = 0;
  while (i < WIDTH - 2 && buff[offset + i] != '\n' &&
         buff[offset + i] != '\0') {
    view_buffer[i] = buff[offset + i];
    i++;
  }
  view_buffer[i] = '\0';
  return i;
}

typedef struct {
  int line;
  int column;

} vec2;

vec2 get_line_col(const char *buf) {
  vec2 v2 = {0, 0};

  for (unsigned long i = 0; buf[i] != '\0'; i++) {
    if (buf[i] == '\n') {
      v2.line++;
      v2.column = 0; // zera coluna
    } else {
      v2.column++;
    }
  }

  return v2;
}
void clear_screen() {
  printf("\033[2J"); // limpa a tela inteira
  printf("\033[H");  // move o cursor para o canto superior esquerdo
  fflush(stdout);    // força o terminal a desenhar
}

int main(int argn, char **argv) {
  if (argn == 1) {
    printf("required file.\n");
    return 1;
  }

  char *file_name = argv[1];
  int f = open(file_name, O_RDONLY);

  if (!f) {
    printf("file doesn't exists.\n");
    return 1;
  }

  int file_size = get_file_size(f);
  char *buffer = malloc(file_size);
  read(f, buffer, file_size);
  close(f);
  enable_raw_mode(&orig_term);

  char inp = 0;

  char view_buffer[80];
  clear_screen();
  build_view(buffer, 0, view_buffer);
  render(view_buffer);
  static int offset = 0;
  SAVE_CURSOR();
  render_info(&editor, &offset);
  while (read(STDIN_FILENO, &inp, 1) == 1 && inp != 'q') {
    switch (inp) {
    case 'l':
    case 'L':
      if (offset == file_size)
        break;
      offset++;
      kidine_change_action(&editor, RIGHT);
      break;
    case 'h':
    case 'H':
      if (offset <= 0)
        break;
      offset--;
      kidine_change_action(&editor, LEFT);
      break;
    default:
      break;
    case 'j':
    case 'J':
      kidine_change_action(&editor, DOWN);
      break;
    case 'k':
    case 'K':
      kidine_change_action(&editor, UP);
      break;
    case 'i':
      kidine_change_mode(&editor, INSERT);
      break;
    case 'I':
    case 27:
      kidine_change_action(&editor, ESCAPE);
      break;
    }
    clear_screen();
    render(view_buffer);
    build_view(buffer, offset, view_buffer);
    render_info(&editor, &offset);
    RESTORE_CURSOR();
    fflush(stdout);
  }
  return 0;
}
