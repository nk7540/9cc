#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
  TK_RESERVED,
  TK_NUM,
  TK_EOF,
} TokenKind;

typedef struct Token Token;
struct Token {
  Token *next;
  TokenKind kind;
  char *str;
  int val;
};

Token *token;

char *user_input;

void error_at(char *loc, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  int pos = loc - user_input;
  fprintf(stderr, "%s\n", user_input);
  fprintf(stderr, "%*s", pos, " ");
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

// For initial tokenization
Token *new_token_after(Token *cur, TokenKind kind, char *str) {
  Token *tok = malloc(sizeof(Token));
  tok->kind = kind;
  tok->str = str;
  cur->next = tok;
  return tok;
}

Token *tokenize(char *p) {
  Token head;
  head.next = NULL;
  Token *cur = &head;
  while (*p) {
    if (isspace(*p)) {
      p++;
      continue;
    }

    if (*p == '+' || *p == '-') {
      cur = new_token_after(cur, TK_RESERVED, p++);
      continue;
    }

    if (isdigit(*p)) {
      cur = new_token_after(cur, TK_NUM, p);
      cur->val = strtol(p, &p, 10);
      continue;
    }

    error_at(p, "cannot be tokenized");
  }

  new_token_after(cur, TK_EOF, p);
  return head.next;
}

// For token processing
bool at_eof() {
  return token->kind == TK_EOF;
}

void pop_token() {
  token = token->next;
}

int pop_number() {
  if (token->kind != TK_NUM)
    error_at(token->str, "number expected, but got something else");
  int val = token->val;
  pop_token();
  return val;
}

bool pop_op_if(char op) {
  if (token->kind != TK_RESERVED || token->str[0] != op)
    return false;
  pop_token();
  return true;
}

void pop_op(char op) {
  if (token->kind != TK_RESERVED || token->str[0] != op)
    error_at(token->str, "invalid operator: %c", token->str);
  pop_token();
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "invalid arg number\n");
    return 1;
  }

  user_input = argv[1];

  printf(".intel_syntax noprefix\n");
  printf(".globl main\n");
  printf("main:\n");

  token = tokenize(argv[1]);
  // "token" is referenced globally below here

  printf("  mov rax, %d\n", pop_number());
  while (!at_eof()) {
    if (pop_op_if('+')) {
      printf("  add rax, %d\n", pop_number());
      continue;
    }

    pop_op('-');
    printf("  sub rax, %d\n", pop_number());
  }

  printf("  ret\n");
  return 0;
}
