%define api.pure full
%code requires {
  typedef int myint;
}
%token NUM
%start s
%%
s : NUM ;
%%
