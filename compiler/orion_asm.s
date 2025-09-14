.section .data
format_int: .string "%d\n"
format_str: .string "%s"
format_float: .string "%.2f\n"
dtype_int: .string "datatype: int\n"
dtype_string: .string "datatype: string\n"
dtype_bool: .string "datatype: bool\n"
dtype_float: .string "datatype: float\n"
dtype_list: .string "datatype: list\n"
dtype_unknown: .string "datatype: unknown\n"
str_true: .string "True\n"
str_false: .string "False\n"
str_index_error: .string "Index Error\n"

.section .text
.global main
.extern printf
.extern orion_malloc
.extern orion_free
.extern exit
.extern fmod
.extern pow
.extern strcmp
.extern list_new
.extern list_from_data
.extern list_len
.extern list_get
.extern list_set
.extern list_append
.extern list_pop
.extern list_insert
.extern list_concat
.extern list_repeat
.extern list_extend
.extern orion_input
.extern orion_input_prompt
.extern int_to_string
.extern float_to_string
.extern bool_to_string
.extern string_to_string
.extern string_concat_parts

main:
    push %rbp
    mov %rsp, %rbp
    sub $64, %rsp
    # Function 'add' defined in scope ''
    # User-defined function call: add
    # Executing function call: add
    # Function prologue: setting up parameters
    mov %rdi, -8(%rbp)  # param a from %rdi
    # Parameter a (type: int) at stack offset -8
    mov %rsi, -16(%rbp)  # param b from %rsi
    # Parameter b (type: int) at stack offset -16
    # Integer binary operation
    mov -8(%rbp), %rax  # load local a
    push %rax
    mov -16(%rbp), %rax  # load local b
    pop %rbx
    add %rbx, %rax
    mov $0, %rax
    add $64, %rsp
    pop %rbp
    ret
