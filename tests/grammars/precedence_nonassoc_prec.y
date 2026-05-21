%token NUM PLUS STAR LT MINUS UMINUS
%left PLUS
%left STAR
%nonassoc LT
%right UMINUS
%start expr
%%
expr
  : expr PLUS expr
  | expr STAR expr
  | expr LT expr
  | MINUS expr %prec UMINUS
  | NUM
  ;
%%
