section .data
format_int: db '%d\n', 0
format_str: db '%s', 0
format_float: db '%.2f\n', 0
dtype_int: db 'datatype: int\n', 0
dtype_string: db 'datatype: string\n', 0
dtype_bool: db 'datatype: bool\n', 0
dtype_float: db 'datatype: float\n', 0
dtype_list: db 'datatype: list\n', 0
dtype_unknown: db 'datatype: unknown\n', 0
str_true: db 'True\n', 0
str_false: db 'False\n', 0
str_index_error: db 'Index Error\n', 0
str_0: db 'Windows assembly test!', 0

section .text
global main
extern printf
extern orion_malloc
extern orion_free
extern exit
extern fmod
extern pow
extern strcmp
extern list_new
extern list_from_data
extern list_len
extern list_get
extern list_set
extern list_append
extern list_pop
extern list_insert
extern list_concat
extern list_repeat
extern list_extend
extern orion_input
extern orion_input_prompt
extern int_to_string
extern float_to_string
extern bool_to_string
extern string_to_string
extern string_concat_parts


fn_main:
    push %rbp
    mov %rsp, %rbp
    sub $64, %rsp  # Allocate stack space for local variables
    # Setting up function parameters for main
    # Call out() with string
    mov $str_0, %rsi
    mov $format_str, %rdi
    xor %rax, %rax
    call printf
    add $64, %rsp  # Restore stack space
    pop %rbp
    ret
main:
    push rbp
    mov rbp, rsp  ; Windows Intel syntax
    sub rsp, 96
    # Function 'main' defined in scope ''
    mov rax, 0  ; Windows Intel syntax
    add rsp, 64
    pop rbp
    ret
