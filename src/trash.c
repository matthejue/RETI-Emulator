#if 0

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char *read_stdin_content() {
  size_t len = 0;
  size_t read;
  char *line = NULL;
  char *content = NULL;

  while ((read = getline(&line, &len, stdin)) != -1) {
    if (content == NULL) {
      content = strdup(line);
    } else {
      content = realloc(content, strlen(content) + read + 1);
      strcat(content, line);
    }
  }

  free(line);
  return content;
}

#define IS_TYPE(var, type) (_Generic((var), type: 1, default: 0))
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// TODO: potentiel neu schreiben, Copilot hat keinen so guten Code generiert
// char **split_string(const char *str, uint8_t *count) {
//   // Copy the input string to avoid modifying the original
//   char *str_copy = strdup(str);
//   if (str_copy == NULL) {
//     fprintf(stderr, "strdup was not successful");
//     exit(EXIT_FAILURE);
//   }
//
//   // Count the number of words
//   int words = 0;
//   char *token = strtok(str_copy, " \t\n");
//   while (token != NULL) {
//     words++;
//     token = strtok(NULL, " \t\n");
//   }
//
//   // Allocate memory for the array of words
//   char **result = malloc((words + 1) * sizeof(void *));
//   if (result == NULL) {
//     fprintf(stderr, "malloc was not successful");
//     exit(EXIT_FAILURE);
//   }
//
//   // Reset the copy of the string
//   strcpy(str_copy, str);
//
//   // Split the string and store the words in the array
//   int idx = 0;
//   token = strtok(str_copy, " \t\n");
//   while (token != NULL) {
//     result[idx] = strdup(token);
//     if (result[idx] == NULL) {
//       fprintf(stderr, "strdup was not successful");
//       exit(EXIT_FAILURE);
//     }
//     idx++;
//     token = strtok(NULL, " \t\n");
//   }
//   result[idx] = NULL;
//
//   // Set the count of words
//   *count = words;
//
//   // Free the copy of the string
//   free(str_copy);
//
//   return result;
// }

// char *read_stdin() {
//   size_t len = 0;
//   size_t read;
//   char *line = NULL;
//   char *content = NULL;
//   while ((read = getline(&line, &len, stdin)) != -1) {
//     if (content == NULL) {
//       content = strdup(line);
//     } else {
//       content = realloc(content, strlen(content) + read + 1);
//       strcat(content, line);
//     }
//   }
//   free(line);
//   return content;
// }

#endif
