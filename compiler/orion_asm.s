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
str_0: .string "enter "

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


mains:
    push %rbp
    mov %rsp, %rbp
    sub $64, %rsp  # Allocate stack space for local variables
    # Setting up function parameters for mains
    mov %rdi, -8(%rbp)  # Parameter a (type: string)
    # Call out(dtype(a))
    mov $dtype_string, %rsi
    mov $format_str, %rdi
    xor %rax, %rax
    call printf
    add $64, %rsp  # Restore stack space
    pop %rbp
    ret
main:
    push %rbp
    mov %rsp, %rbp
    sub $64, %rsp
    # Function 'mains' defined in scope ''
    # Variable: a
    # input() function call
    mov $str_0, %rdi  # Prompt string
    call orion_input_prompt  # Display prompt and read input
    # String address returned in %rax
    mov %rax, -8(%rbp)  # store global a
    # User-defined function call: mains
    # Preparing argument 0
    mov -8(%rbp), %rax  # load global a
    mov %rax, %rdi  # Arg 0 to %rdi
    call mains
    mov $0, %rax
    add $64, %rsp
    pop %rbp
    ret
