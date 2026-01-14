// Initialize Feather icons and line numbers
document.addEventListener('DOMContentLoaded', function() {
    feather.replace();
    initializeLineNumbers();
    updateEditorStats();
});

// Line numbers functionality
function initializeLineNumbers() {
    const editor = document.getElementById('codeEditor');
    const lineNumbers = document.getElementById('lineNumbers');
    
    function updateLineNumbers() {
        const lines = editor.value.split('\n');
        const lineNumbersText = lines.map((_, index) => index + 1).join('\n');
        lineNumbers.textContent = lineNumbersText;
    }
    
    // Update line numbers on input
    editor.addEventListener('input', updateLineNumbers);
    editor.addEventListener('scroll', function() {
        lineNumbers.scrollTop = editor.scrollTop;
    });
    
    // Initial update
    updateLineNumbers();
}

// Update editor statistics
function updateEditorStats() {
    const editor = document.getElementById('codeEditor');
    const lineCount = document.getElementById('lineCount');
    const charCount = document.getElementById('charCount');
    
    function updateStats() {
        const text = editor.value;
        const lines = text.split('\n').length;
        const chars = text.length;
        
        lineCount.textContent = `${lines} lines`;
        charCount.textContent = `${chars} chars`;
    }
    
    editor.addEventListener('input', updateStats);
    updateStats();
}

// Update syntax status
function updateSyntaxStatus(status, message) {
    const syntaxStatus = document.getElementById('syntaxStatus');
    if (!syntaxStatus) return; // Skip if element doesn't exist
    
    const icon = syntaxStatus.querySelector('i');
    const text = syntaxStatus.querySelector('span');
    
    syntaxStatus.className = `syntax-status status-${status}`;
    
    switch(status) {
        case 'valid':
            icon.setAttribute('data-feather', 'check-circle');
            break;
        case 'invalid':
            icon.setAttribute('data-feather', 'x-circle');
            break;
        case 'checking':
            icon.setAttribute('data-feather', 'loader');
            break;
        default:
            icon.setAttribute('data-feather', 'check-circle');
    }
    
    text.textContent = message;
    feather.replace();
}

// Update output status
function updateOutputStatus(status, message) {
    const outputStatus = document.getElementById('outputStatus');
    if (!outputStatus) return; // Skip if element doesn't exist
    
    const dot = outputStatus.querySelector('.status-dot');
    const text = outputStatus.querySelector('.status-text');
    
    dot.className = `status-dot status-${status}`;
    text.textContent = message;
}

// Example programs
const examples = {
    hello: `// Hello World in Orion
fn main() {
    out("Hello, Orion World!")
    out("Fast as C, readable as Python!")
}`,
    
    fibonacci: `// Fibonacci sequence in Orion
fn fibonacci(n) {
    if n <= 1 {
        return n
    }
    return fibonacci(n - 1) + fibonacci(n - 2)
}

fn main() {
    n = 10
    out("Fibonacci sequence:")
    
    for i = 0; i < n; i = i + 1 {
        result = fibonacci(i)
        out("F(" + str(i) + ") = " + str(result))
    }
}`
};

// Load example code
function loadExample(exampleName) {
    const editor = document.getElementById('codeEditor');
    if (examples[exampleName]) {
        editor.value = examples[exampleName];
        initializeLineNumbers();
        updateEditorStats();
        updateSyntaxStatus('ready', 'Ready');
        clearOutput();
        appendOutput('Example loaded: ' + exampleName + '\n', 'info');
        updateOutputStatus('ready', 'Ready');
    }
}

// Clear editor
function clearEditor() {
    document.getElementById('codeEditor').value = '';
    initializeLineNumbers();
    updateEditorStats();
    updateSyntaxStatus('ready', 'Ready');
    clearOutput();
    appendOutput('Editor cleared.\n', 'info');
    updateOutputStatus('ready', 'Ready');
}

// Clear output
function clearOutput() {
    document.getElementById('outputArea').textContent = '';
}

// Append output with styling
function appendOutput(text, type = 'normal') {
    const outputArea = document.getElementById('outputArea');
    const span = document.createElement('span');
    
    switch(type) {
        case 'error':
            span.className = 'status-error';
            break;
        case 'success':
            span.className = 'status-success';
            break;
        case 'warning':
            span.className = 'status-warning';
            break;
        case 'info':
            span.style.color = '#6b7280';
            break;
    }
    
    span.textContent = text;
    outputArea.appendChild(span);
    outputArea.scrollTop = outputArea.scrollHeight;
}

// Set button loading state
function setButtonLoading(buttonId, loading) {
    const button = document.getElementById(buttonId);
    if (!button) return;
    
    const icon = button.querySelector('i');
    
    if (loading) {
        button.disabled = true;
        if (icon) {
            icon.setAttribute('data-feather', 'loader');
            icon.classList.add('spinner-border', 'spinner-border-sm');
        }
        feather.replace();
        
        // Add spinning animation
        const loader = button.querySelector('[data-feather="loader"]');
        if (loader) {
            loader.style.animation = 'spin 1s linear infinite';
        }
    } else {
        button.disabled = false;
        if (icon) {
            icon.classList.remove('spinner-border', 'spinner-border-sm');
            icon.setAttribute('data-feather', 'play');
            icon.style.animation = '';
        }
        feather.replace();
    }
}

// Add spinning keyframe if not already present
if (!document.querySelector('style[data-spinner]')) {
    const style = document.createElement('style');
    style.setAttribute('data-spinner', 'true');
    style.textContent = `
        @keyframes spin {
            from { transform: rotate(0deg); }
            to { transform: rotate(360deg); }
        }
    `;
    document.head.appendChild(style);
}

// Setup syntax highlighting
function setupSyntaxHighlighting() {
    // This function can be used to initialize syntax highlighting
    // For now, we'll keep it simple since we're using a textarea
    console.log('Syntax highlighting setup completed');
}

// Interactive Input Panel Functions
function showInputPanel() {
    const inputPanel = document.getElementById('inputPanel');
    const inputData = document.getElementById('inputData');
    inputPanel.style.display = 'block';
    inputData.focus();
    feather.replace(); // Refresh icons
}

function hideInputPanel() {
    const inputPanel = document.getElementById('inputPanel');
    inputPanel.style.display = 'none';
}

// Run program with provided input data
async function runWithInput() {
    const code = document.getElementById('codeEditor').value.trim();
    const inputData = document.getElementById('inputData').value;
    
    if (!code) {
        appendOutput('Error: No code to compile\n', 'error');
        updateOutputStatus('error', 'Error');
        return;
    }
    
    isCompiling = true;
    clearOutput();
    appendOutput('Running interactive program...\n', 'info');
    appendOutput('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n', 'info');
    appendOutput('Input data provided:\n', 'info');
    appendOutput(inputData.trim() + '\n', 'normal');
    appendOutput('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n', 'info');
    updateOutputStatus('running', 'Running...');
    setButtonLoading('compileBtn', true);
    
    try {
        const response = await fetch('/compile', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ 
                code: code,
                input_data: inputData 
            })
        });
        
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }
        
        const result = await response.json();
        
        if (result.success) {
            hideInputPanel();
            updateOutputStatus('success', 'Success');
            appendOutput('✓ Interactive program completed successfully!\n', 'success');
            appendOutput('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n', 'info');
            appendOutput('Program Output:\n', 'info');
            appendOutput(result.output + '\n', 'normal');
            appendOutput('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n', 'info');
            
            // Show timing breakdown
            const compilationTime = result.compilation_time || 0;
            const executionTime = result.execution_time || 0;
            const totalTime = result.total_time || (compilationTime + executionTime);
            
            appendOutput(`✓ Compilation completed (${compilationTime}ms)\n`, 'success');
            appendOutput(`✓ Execution completed (${executionTime}ms)\n`, 'success');
            appendOutput(`✓ Total runtime: ${totalTime}ms\n`, 'info');
        } else {
            updateOutputStatus('error', 'Error');
            appendOutput('✗ Interactive program failed!\n', 'error');
            appendOutput('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n', 'error');
            appendOutput('Errors:\n', 'error');
            appendOutput(result.error + '\n', 'error');
        }
    } catch (error) {
        console.error('Interactive program error:', error);
        appendOutput('✗ Network error: ' + error.message + '\n', 'error');
        updateOutputStatus('error', 'Network Error');
    } finally {
        setButtonLoading('compileBtn', false);
        isCompiling = false;
    }
}

// Global flag to prevent duplicate compilation requests
let isCompiling = false;

// Compile and run code
async function compileCode() {
    // Prevent duplicate requests
    if (isCompiling) {
        return;
    }
    
    const code = document.getElementById('codeEditor').value.trim();
    
    if (!code) {
        appendOutput('Error: No code to compile\n', 'error');
        updateOutputStatus('error', 'Error');
        return;
    }
    
    isCompiling = true;
    clearOutput();
    appendOutput('Compiling Orion code...\n', 'info');
    updateOutputStatus('running', 'Compiling...');
    setButtonLoading('compileBtn', true);
    
    try {
        const response = await fetch('/compile', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ code: code })
        });
        
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }
        
        const result = await response.json();
        
        if (result.success) {
            hideInputPanel(); // Hide input panel on success
            updateOutputStatus('success', 'Success');
            appendOutput('✓ Compilation successful!\n', 'success');
            appendOutput('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n', 'info');
            appendOutput('Program Output:\n', 'info');
            appendOutput(result.output + '\n', 'normal');
            appendOutput('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n', 'info');
            
            // Show timing breakdown
            const compilationTime = result.compilation_time || 0;
            const executionTime = result.execution_time || 0;
            const totalTime = result.total_time || (compilationTime + executionTime);
            
            appendOutput(`✓ Compilation completed (${compilationTime}ms)\n`, 'success');
            appendOutput(`✓ Execution completed (${executionTime}ms)\n`, 'success');
            appendOutput(`✓ Total runtime: ${totalTime}ms\n`, 'info');
        } else if (result.needs_input) {
            // Show interactive input panel for programs that need input
            updateOutputStatus('waiting', 'Needs Input');
            appendOutput('🔄 Interactive program detected!\n', 'info');
            appendOutput('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n', 'info');
            appendOutput('Your program uses input() and needs data to run.\n', 'info');
            appendOutput('Please provide input values below:\n', 'info');
            showInputPanel();
        } else {
            hideInputPanel(); // Hide input panel on error
            updateOutputStatus('error', 'Error');
            appendOutput('✗ Compilation failed!\n', 'error');
            appendOutput('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n', 'error');
            appendOutput('Errors:\n', 'error');
            appendOutput(result.error + '\n', 'error');
        }
    } catch (error) {
        console.error('Compilation error:', error);
        appendOutput('✗ Network error: ' + error.message + '\n', 'error');
        updateOutputStatus('error', 'Network Error');
    } finally {
        setButtonLoading('compileBtn', false);
        isCompiling = false;
    }
}

// Check syntax
async function checkSyntax() {
    const code = document.getElementById('codeEditor').value.trim();
    
    if (!code) {
        appendOutput('Error: No code to check\n', 'error');
        updateSyntaxStatus('invalid', 'No Code');
        return;
    }
    
    updateSyntaxStatus('checking', 'Checking...');
    clearOutput();
    appendOutput('Checking syntax...\n', 'info');
    
    try {
        const response = await fetch('/check-syntax', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ code: code })
        });
        
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }
        
        const result = await response.json();
        
        if (result.valid) {
            updateSyntaxStatus('valid', 'Valid');
            appendOutput('✓ Syntax is valid!\n', 'success');
            appendOutput('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n', 'info');
            appendOutput('No syntax errors found.\n', 'info');
        } else {
            updateSyntaxStatus('invalid', 'Invalid');
            appendOutput('✗ Syntax errors found!\n', 'error');
            appendOutput('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n', 'error');
            appendOutput(result.error + '\n', 'error');
        }
    } catch (error) {
        console.error('Syntax check error:', error);
        updateSyntaxStatus('invalid', 'Error');
        appendOutput('✗ Error checking syntax: ' + error.message + '\n', 'error');
    }
}

// Show AST
async function showAST() {
    const code = document.getElementById('codeEditor').value.trim();
    
    if (!code) {
        appendOutput('Error: No code to parse\n', 'error');
        return;
    }
    
    clearOutput();
    appendOutput('Generating Abstract Syntax Tree...\n', 'info');
    
    try {
        const response = await fetch('/ast', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ code: code })
        });
        
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }
        
        const result = await response.json();
        
        if (result.success) {
            appendOutput('✓ AST generated successfully!\n', 'success');
            appendOutput('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n', 'info');
            appendOutput('Abstract Syntax Tree:\n', 'info');
            appendOutput(result.ast + '\n', 'normal');
        } else {
            appendOutput('✗ Failed to generate AST!\n', 'error');
            appendOutput('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n', 'error');
            appendOutput(result.error + '\n', 'error');
        }
    } catch (error) {
        console.error('AST generation error:', error);
        appendOutput('✗ Error generating AST: ' + error.message + '\n', 'error');
    }
}

// Keyboard shortcuts
document.addEventListener('keydown', function(e) {
    if (e.ctrlKey || e.metaKey) {
        switch(e.key) {
            case 'Enter':
                e.preventDefault();
                compileCode();
                break;
            case 'k':
                e.preventDefault();
                clearOutput();
                break;
        }
    }
});

// Auto-resize text area
document.getElementById('codeEditor').addEventListener('input', function() {
    // Simple auto-indentation
    if (event.inputType === 'insertLineBreak') {
        const textarea = event.target;
        const lines = textarea.value.split('\n');
        const currentLineIndex = lines.length - 2;
        const currentLine = lines[currentLineIndex];
        
        // Basic indentation logic
        let indent = '';
        if (currentLine.includes('{')) {
            const match = currentLine.match(/^(\s*)/);
            indent = match ? match[1] + '    ' : '    ';
        } else {
            const match = currentLine.match(/^(\s*)/);
            indent = match ? match[1] : '';
        }
        
        if (indent) {
            const cursorPosition = textarea.selectionStart;
            const textBefore = textarea.value.substring(0, cursorPosition);
            const textAfter = textarea.value.substring(cursorPosition);
            textarea.value = textBefore + indent + textAfter;
            textarea.setSelectionRange(cursorPosition + indent.length, cursorPosition + indent.length);
        }
    }
});

// Show features modal
function showFeatures() {
    const modal = new bootstrap.Modal(document.getElementById('helpModal'));
    modal.show();
}

// Examples dropdown functionality
function toggleExamplesDropdown() {
    const dropdown = document.getElementById('examplesDropdown');
    const toggle = dropdown.previousElementSibling;
    const isOpen = dropdown.classList.contains('show');
    
    if (isOpen) {
        closeExamplesDropdown();
    } else {
        openExamplesDropdown();
    }
}

function openExamplesDropdown() {
    const dropdown = document.getElementById('examplesDropdown');
    const toggle = dropdown.previousElementSibling;
    
    dropdown.classList.add('show');
    toggle.classList.add('active');
    
    // Add backdrop to close on outside click
    const backdrop = document.createElement('div');
    backdrop.className = 'dropdown-backdrop';
    backdrop.onclick = closeExamplesDropdown;
    document.body.appendChild(backdrop);
}

function closeExamplesDropdown() {
    const dropdown = document.getElementById('examplesDropdown');
    const toggle = dropdown.previousElementSibling;
    const backdrop = document.querySelector('.dropdown-backdrop');
    
    dropdown.classList.remove('show');
    toggle.classList.remove('active');
    
    if (backdrop) {
        backdrop.remove();
    }
}

// Close dropdown when clicking on an item
document.addEventListener('click', function(e) {
    if (e.target.closest('.dropdown-item')) {
        closeExamplesDropdown();
    }
});

// Enhanced keyboard shortcuts
document.addEventListener('keydown', function(e) {
    if (e.ctrlKey || e.metaKey) {
        switch(e.key) {
            case 'Enter':
                e.preventDefault();
                compileCode();
                break;
            case 'k':
                e.preventDefault();
                clearOutput();
                break;
            case 'C':
                if (e.shiftKey) {
                    e.preventDefault();
                    checkSyntax();
                }
                break;
        }
    }
});

// Initialize when page loads
document.addEventListener('DOMContentLoaded', function() {
    loadExample('hello');
    setupSyntaxHighlighting();
    
    // Add event listeners to prevent unhandled promise rejections
    window.addEventListener('unhandledrejection', function(event) {
        console.error('Unhandled promise rejection:', event.reason);
        event.preventDefault();
    });
});
