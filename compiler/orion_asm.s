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
.extern list_retain
.extern list_release
.extern dict_new
.extern dict_get
.extern dict_set
.extern dict_delete
.extern dict_contains
.extern dict_pop
.extern dict_len
.extern dict_keys
.extern dict_values
.extern dict_items
.extern dict_clear
.extern dict_update
.extern dict_retain
.extern dict_release
.extern orion_input
.extern orion_input_prompt
.extern int_to_string
.extern float_to_string
.extern bool_to_string
.extern string_to_string
.extern string_concat_parts
.extern string_retain
.extern string_release
.extern print_smart
.extern detect_type
.extern range_retain
.extern range_release

main:
    push %rbp
    mov %rsp, %rbp
    sub $64, %rsp
    # Variable: a
    # Enhanced list literal with 9 elements
    # Allocating temporary array for 9 elements
    mov $72, %rdi
    call orion_malloc  # Allocate temporary array
    mov %rax, %r12  # Save temp array pointer in %r12
    # Evaluating element 0
    push %r12  # Save temp array pointer
    mov $1, %rax
    pop %r12  # Restore temp array pointer
    movq %rax, 0(%r12)  # Store in temp array
    # Evaluating element 1
    push %r12  # Save temp array pointer
    mov $2, %rax
    pop %r12  # Restore temp array pointer
    movq %rax, 8(%r12)  # Store in temp array
    # Evaluating element 2
    push %r12  # Save temp array pointer
    mov $3, %rax
    pop %r12  # Restore temp array pointer
    movq %rax, 16(%r12)  # Store in temp array
    # Evaluating element 3
    push %r12  # Save temp array pointer
    mov $4, %rax
    pop %r12  # Restore temp array pointer
    movq %rax, 24(%r12)  # Store in temp array
    # Evaluating element 4
    push %r12  # Save temp array pointer
    mov $5, %rax
    pop %r12  # Restore temp array pointer
    movq %rax, 32(%r12)  # Store in temp array
    # Evaluating element 5
    push %r12  # Save temp array pointer
    mov $6, %rax
    pop %r12  # Restore temp array pointer
    movq %rax, 40(%r12)  # Store in temp array
    # Evaluating element 6
    push %r12  # Save temp array pointer
    mov $7, %rax
    pop %r12  # Restore temp array pointer
    movq %rax, 48(%r12)  # Store in temp array
    # Evaluating element 7
    push %r12  # Save temp array pointer
    mov $8, %rax
    pop %r12  # Restore temp array pointer
    movq %rax, 56(%r12)  # Store in temp array
    # Evaluating element 8
    push %r12  # Save temp array pointer
    mov $9, %rax
    pop %r12  # Restore temp array pointer
    movq %rax, 64(%r12)  # Store in temp array
    mov %r12, %rdi  # Temp array pointer
    mov $9, %rsi  # Element count
    call list_from_data  # Create list from data
    push %rax  # Save list pointer
    mov %r12, %rdi  # Temp array pointer
    call orion_free  # Free temporary array
    pop %rax  # Restore list pointer
    # Storing new list - no retain needed (refcount=1)
    mov %rax, -8(%rbp)  # store global a
    # range() function call
    mov $0, %rax
    mov %rax, %rdi  # Start value as first argument
    push %rdi  # Save start value
    # len() function call
    mov -8(%rbp), %rax  # load global a
    mov %rax, %rdi  # List pointer as argument
    call list_len  # Get list length
    mov %rax, %rsi  # Stop value as second argument
    pop %rdi  # Restore start value
    call range_new_start_stop  # Create range with start and stop
    mov %rax, %r12  # Store iterable pointer
    mov $0, %r13    # Initialize index
    # For-in loop over range object
    mov %r12, %rdi  # Range pointer
    call range_len  # Get range length
    mov %rax, %r14  # Store range length
forin_loop_0:
    cmp %r14, %r13
    jge forin_end_0
    mov %r13, %rax  # Move index to rax
    mov %rax, -16(%rbp)  # i = %rax (type: int)
    # Call out() with indexed access
    # Enhanced index expression with negative indexing support
    mov -8(%rbp), %rax  # load global a
    mov %rax, %rdi  # List pointer as first argument
    mov -16(%rbp), %rax  # load global i
    mov %rax, %rsi  # Index as second argument
    call list_get  # Get element with bounds checking
    mov %rax, %rdi  # Value to print
    call print_smart  # Smart print handles strings and integers
    inc %r13
    jmp forin_loop_0
forin_end_0:
    mov $0, %rax
    add $64, %rsp
    pop %rbp
    ret
