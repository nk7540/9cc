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

typedef enum {
  ND_ADD,
  ND_SUB,
  ND_MUL,
  ND_DIV,
  ND_NUM,
} NodeKind;
typedef struct Node Node;
struct Node {
  Node *lhs;
  Node *rhs;
  NodeKind kind;
  int val;
};

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

// --------------- tokenizer ---------------------
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

    if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '(' || *p == ')') {
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

// ----------------- parser -------------------------
void pop_token() {
  token = token->next;
}

int expect_number() {
  if (token->kind != TK_NUM)
    error_at(token->str, "number expected, but got something else");
  int val = token->val;
  pop_token();
  return val;
}

bool consume(char op) {
  if (token->kind != TK_RESERVED || token->str[0] != op)
    return false;
  pop_token();
  return true;
}

void expect(char op) {
  if (token->kind != TK_RESERVED || token->str[0] != op)
    error_at(token->str, "invalid operator: %c", token->str);
  pop_token();
}

Node *new_node(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = malloc(sizeof(Node));
  node->kind = kind;
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

Node *new_node_num(int num) {
  Node *node = malloc(sizeof(Node));
  node->kind = ND_NUM;
  node->val = num;
  return node;
}

Node *expr();
Node *primary() {
  if (consume('(')) {
    Node *node = expr();
    expect(')');
    return node;
  }

  return new_node_num(expect_number());
}

Node *mul() {
  Node *node = primary();
  for (;;) {
    if (consume('*'))
      node = new_node(ND_MUL, node, primary());
    else if (consume('/'))
      node = new_node(ND_DIV, node, primary());
    else
      return node;
  }
}

Node *expr() {
  Node *node = mul();
  for (;;) {
    if (consume('+'))
      node = new_node(ND_ADD, node, mul());
    else if (consume('-'))
      node = new_node(ND_SUB, node, mul());
    else
      return node;
  }
}

// ---------------- compiler -------------------
void gen(Node *node) {
  if (node->kind == ND_NUM) {
    printf("  push %d\n", node->val);
    return;
  }

  gen(node->lhs);
  gen(node->rhs);
  printf("  pop rdi\n");
  printf("  pop rax\n");
  switch (node->kind) {
    case ND_ADD:
      printf("  add rax, rdi\n");
      break;
    case ND_SUB:
      printf("  sub rax, rdi\n");
      break;
    case ND_MUL:
      printf("  imul rax, rdi\n");
      break;
    case ND_DIV:
      printf("  cqo\n");
      printf("  idiv rdi\n");
      break;
  }
  printf("  push rax\n");
}

// --------------- main ------------------ //
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
  Node *node = expr();
  gen(node);

  printf("  pop rax\n");
  printf("  ret\n");
  return 0;
}
