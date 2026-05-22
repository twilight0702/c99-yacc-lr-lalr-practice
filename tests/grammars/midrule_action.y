%token A B
%start s
%%
s
  : A { $$ = 0; } B
  ;
%%
