#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
  int len;
};

Token *token;

bool startsWith(char *p, char *str) {
  return memcmp(p, str, strlen(str)) == 0;
}

Token *new_token_after(Token *cur, TokenKind kind, char *str, int len) {
  Token *tok = malloc(sizeof(Token));
  tok->kind = kind;
  tok->str = str;
  tok->len = len;
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

    if (startsWith(p, "==") || startsWith(p, "!=") || startsWith(p, "<=") || startsWith(p, ">=")) {
      cur = new_token_after(cur, TK_RESERVED, p, 2);
      p += 2;
      continue;
    }

    if (strchr("+-*/()<>", *p)) {
      cur = new_token_after(cur, TK_RESERVED, p++, 1);
      continue;
    }

    if (isdigit(*p)) {
      cur = new_token_after(cur, TK_NUM, p, 0);
      cur->val = strtol(p, &p, 10);
      continue;
    }

    error_at(p, "cannot be tokenized");
  }

  new_token_after(cur, TK_EOF, p, 0);
  return head.next;
}

// ----------------- utils for parser -------------------------
typedef enum {
  ND_NUM,
  ND_MUL,
  ND_DIV,
  ND_ADD,
  ND_SUB,
  ND_LT,
  ND_LE,
  ND_EQ,
  ND_NE,
} NodeKind;
typedef struct Node Node;
struct Node {
  Node *lhs;
  Node *rhs;
  NodeKind kind;
  int val;
};

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

bool consume(char *op) {
  if (token->kind != TK_RESERVED || strlen(op) != token->len || memcmp(token->str, op, token->len))
    return false;
  pop_token();
  return true;
}

void expect(char *op) {
  if (token->kind != TK_RESERVED || strlen(op) != token->len || memcmp(token->str, op, token->len))
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

// ----------------- parser -------------------------
Node *expr();
Node *primary() {
  if (consume("(")) {
    Node *node = expr();
    expect(")");
    return node;
  }

  return new_node_num(expect_number());
}

Node *unary() {
  if (consume("+"))
    return primary();
  if (consume("-"))
    return new_node(ND_SUB, new_node_num(0), primary());
  return primary();
}

Node *mul() {
  Node *node = unary();
  for (;;) {
    if (consume("*"))
      node = new_node(ND_MUL, node, unary());
    else if (consume("/"))
      node = new_node(ND_DIV, node, unary());
    else
      return node;
  }
}

Node *add() {
  Node *node = mul();
  for (;;) {
    if (consume("+"))
      node = new_node(ND_ADD, node, mul());
    else if (consume("-"))
      node = new_node(ND_SUB, node, mul());
    else
      return node;
  }
}

Node *relational() {
  Node *node = add();
  for (;;) {
    if (consume("<="))
      node = new_node(ND_LE, node, add());
    else if (consume("<"))
      node = new_node(ND_LT, node, add());
    else if (consume(">="))
      node = new_node(ND_LE, add(), node);
    else if (consume(">"))
      node = new_node(ND_LT, add(), node);
    else
      return node;
  }
}

Node *equality() {
  Node *node = relational();
  for (;;) {
    if (consume("=="))
      node = new_node(ND_EQ, node, relational());
    else if (consume("!="))
      node = new_node(ND_NE, node, relational());
    else
      return node;
  }
}

Node *expr() {
  return equality();
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
    case ND_MUL:
      printf("  imul rax, rdi\n");
      break;
    case ND_DIV:
      printf("  cqo\n");
      printf("  idiv rdi\n");
      break;
    case ND_ADD:
      printf("  add rax, rdi\n");
      break;
    case ND_SUB:
      printf("  sub rax, rdi\n");
      break;
    case ND_LT:
      printf("  cmp rax, rdi\n");
      printf("  setl al\n");
      printf("  movzb rax, al\n");
      break;
    case ND_LE:
      printf("  cmp rax, rdi\n");
      printf("  setle al\n");
      printf("  movzb rax, al\n");
      break;
    case ND_EQ:
      printf("  cmp rax, rdi\n");
      printf("  sete al\n");
      printf("  movzb rax, al\n");
      break;
    case ND_NE:
      printf("  cmp rax, rdi\n");
      printf("  setne al\n");
      printf("  movzb rax, al\n");
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
