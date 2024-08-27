#include <stdio.h>
#include <stdlib.h>
#include<string.h>
#include <unistd.h>

#define MAX_BUFFER_SIZE 1024  // Maximum buffer size for reading and writing (1Mb File)
#define MAX_UNDO_REDO 5       // Maximum number of undo and redo operations

typedef struct Operation {
    char *filename;   //Stores the name of a temporary file that holds the state of the file before or after an operation.//
    struct Operation *next;
} Operation;

typedef struct Stack {
    Operation *top;
    int size;
} Stack;

Stack undoStack = {NULL, 0};
Stack redoStack = {NULL, 0};

void push(Stack *stack, const char *filename) {
    if (stack->size >= MAX_UNDO_REDO) {
        Operation *temp = stack->top;
        while (temp->next->next) {
            temp = temp->next;
        }
        free(temp->next->filename);
        free(temp->next);
        temp->next = NULL;
        stack->size--;
    }
    Operation *newOperation = (Operation *)malloc(sizeof(Operation));
    newOperation->filename = strdup(filename);
    newOperation->next = stack->top;
    stack->top = newOperation;
    stack->size++;
}

Operation *pop(Stack *stack) {
    if (stack->top == NULL) return NULL;
    Operation *top = stack->top;
    stack->top = stack->top->next;
    stack->size--;
    return top;
}

void copyFile(const char *src, const char *dest) {
    FILE *source = fopen(src, "r");
    FILE *destination = fopen(dest, "w");
    if (!source || !destination) {
        perror("Unable to open file");
        return;
    }

    char buffer[MAX_BUFFER_SIZE];
    while (fgets(buffer, MAX_BUFFER_SIZE, source) != NULL) {
        fputs(buffer, destination);
    }

    fclose(source);
    fclose(destination);
}

void displayFile(const char *filename) {
    FILE *file = fopen(filename, "r"); // Open file in read mode
    if (file == NULL) { // Check if the file was opened successfully
        perror("Unable to open file"); // Print error message if file could not be opened
        return;
    }

    char buffer[MAX_BUFFER_SIZE]; // Buffer to hold lines read from the file
    while (fgets(buffer, MAX_BUFFER_SIZE, file) != NULL) { // Read lines from the file
        printf("%s", buffer); // Print each line
    }

    fclose(file);  // Close the file
}

void editFile(const char *filename) {
    char tempFilename[] = "tempfileXXXXXX";
    int fd = mkstemp(tempFilename);
    if (fd == -1) {
        perror("Unable to create temporary file");
        return;
    }
    close(fd);

    copyFile(filename, tempFilename);
    push(&undoStack, tempFilename);

    FILE *file = fopen(filename, "w"); // Open file in write mode
    if (file == NULL) {
        perror("Unable to open file"); // Print error message if file could not be opened
        return;
    }

    char buffer[MAX_BUFFER_SIZE]; // Buffer to hold user input

    printf("Enter new content (end with a single period '.'): \n");
    while (fgets(buffer, MAX_BUFFER_SIZE, stdin) != NULL) { // Read user input
        if (strcmp(buffer, ".\n") == 0) { // Check if the input is a single period
            break; // Stop reading input
        }
        fputs(buffer, file); // Write user input to the file
    }

    fclose(file); // Close the file
}

void undo(const char *filename) {
    Operation *operation = pop(&undoStack);
    if (operation == NULL) {
        printf("Nothing to undo.\n");
        return;
    }

    char tempFilename[] = "tempfileXXXXXX";
    int fd = mkstemp(tempFilename);
    if (fd == -1) {
        perror("Unable to create temporary file");
        free(operation->filename);
        free(operation);
        return;
    }
    close(fd);

    copyFile(filename, tempFilename);
    push(&redoStack, tempFilename);

    copyFile(operation->filename, filename);
    remove(operation->filename);

    free(operation->filename);
    free(operation);
}

void redo(const char *filename) {
    Operation *operation = pop(&redoStack);
    if (operation == NULL) {
        printf("Nothing to redo.\n");
        return;
    }

    char tempFilename[] = "tempfileXXXXXX";
    int fd = mkstemp(tempFilename);
    if (fd == -1) {
        perror("Unable to create temporary file");
        free(operation->filename);
        free(operation);
        return;
    }
    close(fd);

    copyFile(filename, tempFilename);
    push(&undoStack, tempFilename);

    copyFile(operation->filename, filename);
    remove(operation->filename);

    free(operation->filename);
    free(operation);
}

void searchWord(const char *filename, const char *word) {
    FILE *file = fopen(filename, "r"); // Open file in read mode
    if (file == NULL) { // Check if the file was opened successfully
        perror("Unable to open file"); // Print error message if file could not be opened
        return;
    }

    char buffer[MAX_BUFFER_SIZE]; // Buffer to hold lines read from the file
    int line_number = 1;
    int found = 0;

    while (fgets(buffer, MAX_BUFFER_SIZE, file) != NULL) { // Read lines from the file
        char *pos = strstr(buffer, word);
        if (pos) {
            found = 1;
            printf("Found '%s' at line %d, position %ld\n", word, line_number, pos - buffer + 1);
        }
        line_number++;
    }

    if (!found) {
        printf("Word '%s' not found in the file.\n", word);
    }

    fclose(file);  // Close the file
}

void replaceWord(const char *filename, const char *oldWord, const char *newWord) {
    char tempFilename[] = "tempfileXXXXXX";
    int fd = mkstemp(tempFilename);
    if (fd == -1) {
        perror("Unable to create temporary file");
        return;
    }
    close(fd);

    copyFile(filename, tempFilename);
    push(&undoStack, tempFilename);

    FILE *file = fopen(filename, "r");
    FILE *tempFile = fopen("tempfile.tmp", "w");
    if (!file || !tempFile) {
        perror("Unable to open file");
        return;
    }

    char buffer[MAX_BUFFER_SIZE];
    while (fgets(buffer, MAX_BUFFER_SIZE, file) != NULL) {
        char *pos = strstr(buffer, oldWord);
        while (pos) {
            long int len = pos - buffer;
            buffer[len] = '\0';
            fprintf(tempFile, "%s%s", buffer, newWord);
            pos += strlen(oldWord);
            memmove(buffer, pos, strlen(pos) + 1);
            pos = strstr(buffer, oldWord);
        }
        fprintf(tempFile, "%s", buffer);
    }

    fclose(file);
    fclose(tempFile);

    remove(filename);
    rename("tempfile.tmp", filename);
}

void insertWord(const char *filename, const char *word, int line, int pos) {
    char tempFilename[] = "tempfileXXXXXX";
    int fd = mkstemp(tempFilename);
    if (fd == -1) {
        perror("Unable to create temporary file");
        return;
    }
    close(fd);

    copyFile(filename, tempFilename);
    push(&undoStack, tempFilename);

    FILE *file = fopen(filename, "r");
    FILE *tempFile = fopen("tempfile.tmp", "w");
    if (!file || !tempFile) {
        perror("Unable to open file");
        return;
    }

    char buffer[MAX_BUFFER_SIZE];
    int current_line = 1;
    while (fgets(buffer, MAX_BUFFER_SIZE, file) != NULL) {
        if (current_line == line) {
            long int len = strlen(buffer);
            if (pos > len) pos = len;
            buffer[pos] = '\0';
            fprintf(tempFile, "%s%s%s", buffer, word, buffer + pos);
        } else {
            fputs(buffer, tempFile);
        }
        current_line++;
    }

    fclose(file);
    fclose(tempFile);

    remove(filename);
    rename("tempfile.tmp", filename);
}

void modifyLine(const char *filename, const char *newContent, int line, int startPos) {
    char tempFilename[] = "tempfileXXXXXX";
    int fd = mkstemp(tempFilename);
    if (fd == -1) {
        perror("Unable to create temporary file");
        return;
    }
    close(fd);

    copyFile(filename, tempFilename);
    push(&undoStack, tempFilename);

    FILE *file = fopen(filename, "r");
    FILE *tempFile = fopen("tempfile.tmp", "w");
    if (!file || !tempFile) {
        perror("Unable to open file");
        return;
    }

    char buffer[MAX_BUFFER_SIZE];
    int current_line = 1;
    while (fgets(buffer, MAX_BUFFER_SIZE, file) != NULL) {
        if (current_line == line) {
            long int len = strlen(buffer);
            if (startPos > len) startPos = len;
            buffer[startPos] = '\0';
            fprintf(tempFile, "%s%s", buffer, newContent);
        } else {
            fputs(buffer, tempFile);
        }
        current_line++;
    }

    fclose(file);
    fclose(tempFile);

    remove(filename);
    rename("tempfile.tmp", filename);
}

int main(void) {
    char filename[100]; // Buffer to hold the filename
    int choice; // Variable to hold the user's menu choice

    printf("Enter the filename: ");
    scanf("%s", filename);
    getchar(); // Consume the newline character left by scanf

    while (1) {
        printf("\nFile Editor Menu:\n");
        printf("1. Display File\n");
        printf("2. Edit File\n");
        printf("3. Undo\n");
        printf("4. Redo\n");
        printf("5. Search Word\n");
        printf("6. Replace Word\n");
        printf("7. Insert Word\n");
        printf("8. Modify Line\n");
        printf("9. Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);
        getchar(); // Consume the newline character left by scanf

        switch (choice) {
            case 1:
                displayFile(filename); // Display the file
                break;
            case 2:
                editFile(filename); // Edit the file
                break;
            case 3:
                undo(filename); // Undo the last operation
                break;
            case 4:
                redo(filename); // Redo the last undone operation
                break;
            case 5: {
                char word[100];
                printf("Enter the word to search: ");
                scanf("%s", word);
                searchWord(filename, word); // Search for the specified word
                break;
            }
            case 6: {
                char oldWord[100], newWord[100];
                printf("Enter the word to replace: ");
                scanf("%s", oldWord);
                printf("Enter the new word: ");
                scanf("%s", newWord);
                replaceWord(filename, oldWord, newWord); // Replace the specified word
                break;
            }
            case 7: {
                char word[100];
                int line, pos;
                printf("Enter the word to insert: ");
                scanf("%s", word);
                printf("Enter the line number: ");
                scanf("%d", &line);
                printf("Enter the position: ");
                scanf("%d", &pos);
                insertWord(filename, word, line, pos); // Insert the word at the specified position
                break;
            }
            case 8: {
                char newContent[MAX_BUFFER_SIZE];
                int line, startPos;
                printf("Enter the new content: ");
                getchar(); // Consume the newline character left by previous input
                fgets(newContent, MAX_BUFFER_SIZE, stdin);
                newContent[strcspn(newContent, "\n")] = 0; // Remove the newline character
                printf("Enter the line number: ");
                scanf("%d", &line);
                printf("Enter the start position: ");
                scanf("%d", &startPos);
                modifyLine(filename, newContent, line, startPos); // Modify the line starting from the specified position
                break;
            }
            case 9:
                exit(0); // Exit the program
            default:
                printf("Invalid choice. Please try again.\n"); // Handle invalid menu choices
        }
    }

    return 0;
}

