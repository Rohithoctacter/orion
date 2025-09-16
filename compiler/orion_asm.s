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
str_0: .string "Hello, Orion World!"
str_1: .string "Welcome to the fast and readable programming language!"
str_2: .string "Developer"
str_3: .string "Hello, "
str_4: .string "!"
str_5: .string "Current year: "

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


fn_main:
    push %rbp
    mov %rsp, %rbp
    sub $64, %rsp
    # Setting up function parameters for main
    # Call out() with string
    leaq str_0(%rip), %rsi
    leaq format_str(%rip), %rdi
    xor %rax, %rax
    call printf
    # Call out() with string
    leaq str_1(%rip), %rsi
    leaq format_str(%rip), %rdi
    xor %rax, %rax
    call printf
    # Variable: name
    mov $str_2, %rax
    mov %rax, -8(%rbp)
    # store local name
    # Integer binary operation
    # Integer binary operation
    mov $str_3, %rax
    push %rax
    mov -8(%rbp), %rax  # load local name
    pop %rbx
    add %rbx, %rax
    push %rax
    mov $str_4, %rax
    pop %rbx
    add %rbx, %rax
    # Call out() with expression result
    mov %rax, %rsi
    mov $format_int, %rdi
    xor %rax, %rax
    call printf
    # Variable: year
    mov $2025, %rax
    mov %rax, -16(%rbp)
    # store local year
    # Integer binary operation
    mov $str_5, %rax
    push %rax
    # str() type conversion function call
    mov -16(%rbp), %rax  # load local year
    mov %rax, %rdi  # int variable
    call __orion_int_to_string
    pop %rbx
    add %rbx, %rax
    # Call out() with expression result
    mov %rax, %rsi
    mov $format_int, %rdi
    xor %rax, %rax
    call printf
    add $64, %rsp
    pop %rbp
    ret
main:
    push %rbp
    mov %rsp, %rbp
    sub $64, %rsp
    # Function 'main' defined in scope ''
    mov $0, %rax
    add $64, %rsp
    pop %rbp
    ret
