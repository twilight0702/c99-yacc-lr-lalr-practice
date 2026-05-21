%union {
  int ival;
}
%token <ival> NUM
%type <ival> expr
%start expr
%%
expr
  : NUM
  ;
