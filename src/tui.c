// #include <ncurses.h>
// #include <string.h>
// #include <stdlib.h>
//
// // Define box properties
// typedef struct {
//     int x, y;       // Position (top-left corner)
//     int width, height;  // Size of the box
// } Box;
//
// // Function to draw a box at specified position and size
// void draw_box(Box box, const char* title) {
//     mvprintw(box.y, box.x + (box.width - strlen(title)) / 2, title); // Print title centered
//     mvhline(box.y + 1, box.x, 0, box.width);      // Top border
//     mvhline(box.y + box.height, box.x, 0, box.width);  // Bottom border
//     mvvline(box.y + 1, box.x, 0, box.height);     // Left border
//     mvvline(box.y + 1, box.x + box.width, 0, box.height);  // Right border
//     mvaddch(box.y + 1, box.x, ACS_ULCORNER);       // Top-left corner
//     mvaddch(box.y + 1, box.x + box.width, ACS_URCORNER);  // Top-right corner
//     mvaddch(box.y + box.height, box.x, ACS_LLCORNER); // Bottom-left corner
//     mvaddch(box.y + box.height, box.x + box.width, ACS_LRCORNER); // Bottom-right corner
// }
//
// // Helper function to wrap only lines that exceed box width
// char** wrap_or_preserve_lines(const char* text, int width, int* line_count) {
//     char* text_copy = strdup(text);
//     char* line = strtok(text_copy, "\n");
//     char** result_lines = malloc(200 * sizeof(char*));  // Allocate memory for 200 lines
//     *line_count = 0;
//
//     while (line) {
//         int line_len = strlen(line);
//
//         if (line_len > width) {
//             // Wrap line if it exceeds width
//             int i = 0;
//             while (i < line_len) {
//                 int segment_len = (line_len - i < width) ? line_len - i : width;
//                 result_lines[(*line_count)++] = strndup(line + i, segment_len);
//                 i += segment_len;
//             }
//         } else {
//             // Preserve line if it fits within width
//             result_lines[(*line_count)++] = strdup(line);
//         }
//
//         line = strtok(NULL, "\n");
//     }
//
//     free(text_copy);
//     return result_lines;
// }
//
// // Function to add text within a box and scroll if it exceeds box height
// void add_text_in_box(Box box, const char* text) {
//     int line_count;
//     char** lines = wrap_or_preserve_lines(text, box.width - 2, &line_count);
//
//     // Center the visible lines if content exceeds box height
//     int start_line = 0;
//     if (line_count > box.height - 2) {
//         start_line = (line_count - (box.height - 2)) / 2;  // Center text vertically
//     }
//
//     for (int i = 0; i < box.height - 2 && start_line + i < line_count; i++) {
//         mvprintw(box.y + 2 + i, box.x + 2, "%s", lines[start_line + i]);  // Print within box
//     }
//
//     // Free allocated memory for lines
//     for (int i = 0; i < line_count; i++) {
//         free(lines[i]);
//     }
//     free(lines);
// }

// int main() {
//     // Initialize ncurses
//     initscr();
//     cbreak();
//     noecho();
//     curs_set(0);  // Hide cursor
//
//     // Define boxes
//     Box box1 = {2, 2, 40, 5};      // Position (x, y) and Size (width, height)
//     Box box2 = {45, 2, 30, 8};
//     Box box3 = {2, 10, 73, 5};
//
//     // Draw boxes with titles
//     draw_box(box1, "Box 1");
//     draw_box(box2, "Box 2");
//     draw_box(box3, "Box 3");
//
//     // Text with multiple lines
//     const char* long_text = 
//         "This line is short.\n"
//         "This line, however, is quite long and should wrap to fit within the box.\n"
//         "Another short line.\n"
//         "Yet another very long line that should wrap to fit inside the box dimensions properly.\n"
//         "Last short line.";
//
//     // Add text to boxes
//     add_text_in_box(box1, long_text);
//     add_text_in_box(box2, long_text);
//     add_text_in_box(box3, long_text);
//
//     // Refresh to display changes
//     refresh();
//
//     // Wait for user input to exit
//     getch();
//
//     // End ncurses mode
//     endwin();
//     return 0;
// }
