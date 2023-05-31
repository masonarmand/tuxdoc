#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_LINE_LENGTH 1024

typedef struct {
        char* brief;
        char* returns;
        char* desc;
        char** params;
        char* prototype;
        char* name;
        unsigned int num_params;
} FunctionDoc;

typedef struct {
        FunctionDoc** docs;
        unsigned int length;
} FunctionDocList;


FunctionDocList* generate_documentation(const char* filename);
void generate_txt_files(FunctionDocList list, const char* dir);
void doc_to_txt(FunctionDoc doc, const char* dir);
void create_website(FunctionDocList list, char* title);
void process_file(const char* filename);
char* append_extension(const char* filename, const char* ext);
char* get_function_name(const char* prototype);
void free_doc_list(FunctionDocList* doc_list);
bool is_comment(const char* str);
bool is_whitespace(const char* str);
bool str_starts_with(const char* str, const char* prefix);
char* get_stripped_line(char* line);
void clean_str(char* str);
void add_param_to_doc(FunctionDoc* doc, char* param);
char* get_content_after_val(char* line, char* value);
void append_function(FunctionDocList* list, FunctionDoc* doc);
FunctionDoc* init_function_doc(void);
void remove_dir(const char* dirpath);


int main(int argc, char** argv)
{
        FunctionDocList* doc_list = NULL;

        if (argc != 4) {
                fprintf(stderr, "Usage: %s <C source file> <output directory> <project name>\n", argv[0]);
                return 0;
        }

        remove_dir(argv[2]);
        mkdir(argv[2], 0755);

        doc_list = generate_documentation(argv[1]);
        generate_txt_files(*doc_list, argv[2]);
        create_website(*doc_list, argv[3]);
        free_doc_list(doc_list);

        return 0;
}


FunctionDocList* generate_documentation(const char* filename)
{
        FunctionDocList* doc_list = malloc(sizeof(FunctionDocList));
        FunctionDoc* current_doc = NULL;
        FILE* file = fopen(filename, "r");
        char line[MAX_LINE_LENGTH];

        doc_list->docs = NULL;
        doc_list->length = 0;

        while (fgets(line, MAX_LINE_LENGTH, file)) {
                char* content;
                if (is_comment(line) && strstr(line, "lua function") != NULL) {
                        current_doc = init_function_doc();
                        continue;
                }

                if (current_doc == NULL)
                        continue;

                if (strstr(line, "*/") != NULL) {
                        append_function(doc_list, current_doc);
                        current_doc = NULL;
                        continue;
                }

                content = get_stripped_line(line);
                if (str_starts_with(content, "@brief")) {
                        current_doc->brief = strdup(content + strlen("@brief"));
                }
                else if (str_starts_with(content, "@param")) {
                        char* param = strdup(content + strlen("@param"));
                        add_param_to_doc(current_doc, param);
                }
                else if (str_starts_with(content, "@returns")) {
                        current_doc->returns = strdup(content + strlen("@returns"));
                }
                else if (str_starts_with(content, "@usage")) {
                        current_doc->prototype = strdup(content + strlen("@usage"));
                        current_doc->name = get_function_name(current_doc->prototype);
                }
                else if (str_starts_with(content, "@")) {
                        fprintf(stderr, "Unknown symbol : %s\n", content);
                        exit(1);
                }
                else if (!is_whitespace(content)) {
                        if (current_doc->desc == NULL) {
                                current_doc->desc = strdup(content);
                        }
                        else {
                                unsigned int old_len = strlen(current_doc->desc);
                                unsigned int new_len = old_len + strlen(content) + 1;
                                current_doc->desc = realloc(current_doc->desc, new_len + 1);
                                strcpy(current_doc->desc + old_len, " ");
                                strcpy(current_doc->desc + old_len + 1, content);
                        }
                }
                free(content);
        }

        fclose(file);

        return doc_list;
}


void generate_txt_files(FunctionDocList list, const char* dir)
{
        unsigned int i;
        if (chdir(dir) == -1) {
                perror("chdir");
                exit(1);
        }

        for (i = 0; i < list.length; i++) {
                doc_to_txt(*list.docs[i], dir);
        }
}


void doc_to_txt(FunctionDoc doc, const char* dir)
{
        char* funcname = get_function_name(doc.prototype);
        char* filename = append_extension(funcname, ".txt");
        FILE* file = fopen(filename, "w");
        unsigned int i;

        printf("Creating txt file: %s in directory: %s...\n", filename, dir);

        fprintf(file, "-----\n");
        fprintf(file, "title: %s\n", funcname);
        fprintf(file, "date: \n");
        fprintf(file, "description: \n");
        fprintf(file, "tags: \n");
        fprintf(file, "-----\n");

        fprintf(file, "# Function %s\n", funcname);
        fprintf(file, "```\n");
        fprintf(file, "%s\n", doc.prototype);
        fprintf(file, "```\n");
        fprintf(file, "%s\n\n", doc.brief);
        if (doc.desc != NULL)
                fprintf(file, "%s\n", doc.desc);
        fprintf(file, "## Parameters\n");

        for (i = 0; i < doc.num_params; i++) {
                fprintf(file, "- %s\n\n", doc.params[i]);
        }

        fprintf(file, "## Returns\n");
        fprintf(file, "%s\n", doc.returns);

        fclose(file);
        free(funcname);
        free(filename);
}


void create_website(FunctionDocList list, char* title)
{
        FILE* index_file = fopen("index.html", "w");
        unsigned int i;

        mkdir("docs", 0755);

        for (i = 0; i < list.length; i++) {
                char* filename = append_extension(list.docs[i]->name, ".txt");
                process_file(list.docs[i]->name);
                remove(filename);
                free(filename);
        }
        fprintf(index_file,
                "<!DOCTYPE html>\n"
                "<html lang='en'>\n"
                "<head>\n"
                "  <meta charset='UTF-8'>\n"
                "  <meta name='viewport' content='width=device-width, initial-scale=1'>\n"
                "  <link href='/style.css' rel='stylesheet' type='text/css' media='all'>\n"
                "  <title>%s Documentation</title>\n"
                "</head>\n"
                "<body translate='no'>\n"
                "  <main class='front'>\n"
                "  <div class='nav'>\n",
                title);
        for (i = 0; i < list.length; i++) {
                fprintf(index_file, "    <a href='/docs/%s.html' target='docs-window'>%s</a>\n", list.docs[i]->name, list.docs[i]->name);
        }
        fprintf(index_file,
                "  </div>\n"
                "  <div class='content'>\n"
                "    <iframe name='docs-window' src='homepage.html'></iframe>\n"
                "  </div>\n"
                "  </main>\n"
                "</body>\n"
                "</html>\n");

        fclose(index_file);
}


void process_file(const char* name)
{
        char* filename = append_extension(name, ".txt");
        char* html_filename = append_extension(name, ".html");
        char* output_filename = append_extension("docs/", html_filename);
        pid_t pid = fork();

        if (pid == -1) {
                perror("fork");
                exit(1);
        }

        if (pid == 0) {
                const char* argv[] = { "txt2web", NULL, NULL, NULL };
                argv[1] = filename;
                argv[2] = output_filename;
                execvp("txt2web", (char* const*) argv);
                perror("execvp");
                exit(1);
        }
        else {
                int status;
                waitpid(pid, &status, 0);
        }

        free(html_filename);
        free(filename);
        free(output_filename);
}


char* append_extension(const char* filename, const char* ext)
{
        unsigned int len = strlen(filename) + strlen(ext) + 1;
        char* result = malloc(len * sizeof(char));

        if (result == NULL) {
                fprintf(stderr, "Failed to allocated memory for filename.\n");
                exit(1);
        }

        strcpy(result, filename);
        strcat(result, ext);
        return result;
}


char* get_function_name(const char* prototype)
{
        char* pos = strchr(prototype, '(');
        char* result;
        unsigned int len;

        if (pos == NULL)
                return strdup(prototype);

        len = pos - prototype;
        result = malloc((len + 1) * sizeof(char));
        strncpy(result, prototype, len);
        result[len] = '\0';
        clean_str(result);
        return result;
}


void free_doc_list(FunctionDocList* doc_list)
{
        unsigned int i;
        if (doc_list == NULL)
                return;
        for (i = 0; i < doc_list->length; i++) {
                FunctionDoc* doc = doc_list->docs[i];
                if (doc != NULL) {
                        unsigned int j;
                        free(doc->brief);
                        free(doc->desc);
                        free(doc->returns);
                        free(doc->prototype);
                        free(doc->name);
                        for (j = 0; j < doc->num_params; j++) {
                                free(doc->params[j]);
                        }
                        free(doc->params);
                        free(doc);
                }
        }
        free(doc_list->docs);
        free(doc_list);
}


bool is_comment(const char* str)
{
        if (strstr(str, "*") != NULL)
                return true;
        return false;
}


bool is_whitespace(const char* str)
{
        while (*str != '\0') {
                if (!isspace((unsigned char)*str)) {
                        return false;
                }
                str++;
        }
        return true;
}


bool str_starts_with(const char* str, const char* prefix)
{
        return strncmp(prefix, str, strlen(prefix)) == 0;
}


char* get_stripped_line(char* line)
{
        char* content = strstr(line, "*");
        char* stripped;
        if (content == NULL) {
                fprintf(stderr, "Failed to get content inside of docstring : %s\n", line);
                exit(1);
        }
        content++;
        stripped = strdup(content);
        clean_str(stripped);
        return stripped;
}

void clean_str(char* str)
{
        /* find first non-whitespace char */
        char* original_str = str;
        while(isspace((unsigned char)*str)) str++;

        /* create new string without the leading whitespace */
        memmove(original_str, str, strlen(str) + 1);

        original_str[strcspn(original_str, "\n")] = 0; /* remove newline */
}


void add_param_to_doc(FunctionDoc* doc, char* param)
{
        if (doc->params == NULL) {
                doc->params = malloc(sizeof(char*));
        }
        else {
                doc->params = realloc(doc->params, (doc->num_params + 1) * sizeof(char*));
        }

        if (doc->params == NULL) {
                printf("Failed to allocate memory for params list\n");
                exit(1);
        }

        doc->params[doc->num_params] = param;
        doc->num_params++;
}


void append_function(FunctionDocList* list, FunctionDoc* doc)
{
        FunctionDoc** temp = realloc(list->docs, sizeof(FunctionDoc*) * (list->length + 1));
        if (temp == NULL) {
                fprintf(stderr, "Failed to allocate memory for FunctionDoc\n");
                exit(1);
        }

        list->docs = temp;
        list->docs[list->length] = doc;
        list->length++;
}


FunctionDoc* init_function_doc(void)
{
        FunctionDoc* new_doc = (FunctionDoc*) malloc(sizeof(FunctionDoc));

        if (new_doc == NULL) {
                fprintf(stderr, "Failed to allocate memory for FunctionDoc\n");
                exit(1);
        }

        new_doc->desc = NULL;
        new_doc->brief = NULL;
        new_doc->prototype = NULL;
        new_doc->params = NULL;
        new_doc->num_params = 0;
        new_doc->returns = NULL;

        return new_doc;
}


void remove_dir(const char* dirpath)
{
        DIR* dir = opendir(dirpath);
        struct dirent* entry;
        char path[1024];
        struct stat st;

        if (dir == NULL)
                return;

        while ((entry = readdir(dir)) != NULL) {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                        continue;
                }

                snprintf(path, sizeof(path), "%s/%s", dirpath, entry->d_name);
                if (stat(path, &st) == -1) {
                        continue;
                }

                if (S_ISDIR(st.st_mode)) {
                        remove_dir(path);
                        rmdir(path);
                }
                else if (S_ISREG(st.st_mode)) {
                        remove(path);
                }
        }
        closedir(dir);
}
