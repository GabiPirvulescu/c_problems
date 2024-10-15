#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#define __DEBUG

#ifdef __DEBUG
void debug_info(const char *file, const char *function, const int line)
{
    fprintf(stderr, "DEBUG. ERROR PLACE: File=\"%s\", Function=\"%s\", Line=\"%d\"\n", file, function, line);
}

#define ERR_MSG(DBG_MSG)                              \
    {                                                 \
        perror(DBG_MSG);                              \
        debug_info(__FILE__, __FUNCTION__, __LINE__); \
    }

#else

#define ERR_MSG(DBG_MSG) \
    {                    \
        perror(DBG_MSG); \
    }

#endif

#define PATH_LEN 102400
#define MAGIC "KlxO"
#define HEADER_SIZE 2
#define VERSION 4
#define NO_OF_SECTIONS 2

typedef struct
{
    char sect_name[19];
    int sect_type;
    int sect_offset;
    int sect_size;
    int nr_lines;
} section_file;

typedef struct
{
    section_file *sections;
    int check;
    int version;
    int nr_sections;
} is_SF_file;

void list_contents(char *path, int option)
{
    DIR *dir;
    struct dirent *entry;
    struct stat inode;

    dir = opendir(path);
    if (dir == 0)
    {
        ERR_MSG("Error opening directory"); // error when opening directory
        exit(2);
    }
    bool found = false;
    while ((entry = readdir(dir)) != 0) // open directory successfully
    {
        char contentPath[PATH_LEN];
        snprintf(contentPath, PATH_LEN, "%s/%s", path, entry->d_name); // build the complete path to the element in the directory

        if ((stat(contentPath, &inode)) == -1)
        {
            perror("stat in listing");
            exit(3);
        }

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        if (option == 1) // has_perm_write option
        {

            if (S_ISDIR(inode.st_mode) && (inode.st_mode & S_IWUSR)) // check directory
            {
                if (found == false)
                {
                    printf("SUCCESS\n");
                    found = true;
                }
                printf("%s\n", contentPath);
            }
            if (S_ISREG(inode.st_mode) && (inode.st_mode & S_IWUSR)) // check reg_file
            {
                if (found == false)
                {
                    printf("SUCCESS\n");
                    found = true;
                }
                printf("%s\n", contentPath);
            }
        }
        else if (option > 1) // size option
        {
            if (S_ISREG(inode.st_mode) && inode.st_size < option)
            {
                if (found == false)
                {
                    printf("SUCCESS\n");
                    found = true;
                }
                printf("%s\n", contentPath);
            }
        }
        else if (option == 0) // without any criteria
        {
            if (found == false)
            {
                printf("SUCCESS\n");
                found = true;
            }
            printf("%s\n", contentPath);
        }
    }
    closedir(dir);
}

void list_contents_recursively(char *path, int option, bool *found)
{
    DIR *dir;
    struct stat inode;
    struct dirent *entry;

    dir = opendir(path);
    if (dir == 0)
    {
        perror("ERROR");
        ERR_MSG("invalid directory path"); // error when opening directory
        exit(4);
    }

    while ((entry = readdir(dir)) != 0)
    {
        char contentPath[PATH_LEN];
        snprintf(contentPath, PATH_LEN, "%s/%s", path, entry->d_name); // build the complete path to the element in the directory

        if ((stat(contentPath, &inode)) == -1)
        {
            perror("stat in listing");
            exit(3);
        }

        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        {
            if (option == 1)
            {
                if (S_ISREG(inode.st_mode) && (inode.st_mode & S_IWUSR))
                {
                    if (*found == false)
                    {
                        printf("SUCCESS\n");
                        *found = true;
                    }
                    printf("%s\n", contentPath);
                }
                else if (S_ISDIR(inode.st_mode) && (inode.st_mode & S_IWUSR))
                {
                    if (*found == false)
                    {
                        printf("SUCCESS\n");
                        *found = true;
                    }
                    printf("%s\n", contentPath);
                    list_contents_recursively(contentPath, option, found);
                }
            }
            else if (option > 1)
            {
                if (S_ISREG(inode.st_mode))
                {
                    int fd = open(contentPath, O_RDONLY);
                    int size;
                    if ((size = lseek(fd, 0, SEEK_END)) == -1)
                    {
                        perror("error reposition pointer");
                        exit(1);
                    }
                    if (size < option)
                    {
                        if (*found == false)
                        {
                            printf("SUCCESS\n");
                            *found = true;
                        }
                        printf("%s\n", contentPath);
                    }
                }
                else if (S_ISDIR(inode.st_mode))
                {
                    list_contents_recursively(contentPath, option, found);
                }
            }
            else if (option == 0)
            {
                if (S_ISREG(inode.st_mode))
                {
                    if (*found == false)
                    {
                        printf("SUCCESS\n");
                        *found = true;
                    }
                    printf("%s\n", contentPath);
                }
                else if (S_ISDIR(inode.st_mode))
                {
                    if (*found == false)
                    {
                        printf("SUCCESS\n");
                        *found = true;
                    }
                    printf("%s\n", contentPath);
                    list_contents_recursively(contentPath, option, found);
                }
            }
        }
    }
    closedir(dir);
}

void process_list_command(int argc, char **argv)
{
    bool recursive = false;
    int option = 0; // encoding the options: 1 represents the permission and the value represents the size option
    char path[PATH_LEN];
    for (int i = 2; i < argc; i++)
    {
        if (strcmp(argv[i], "recursive") == 0)
        {
            recursive = true;
        }
        else if (strncmp(argv[i], "size", 4) == 0)
        {
            option = atoi(argv[i] + 13);
        }
        else if (strcmp(argv[i], "has_perm_write") == 0)
        {
            option = 1;
        }
        else if (strncmp(argv[i], "path", 4) == 0)
        {
            strcpy(path, argv[i] + 5);
            path[PATH_LEN] = '\0';
        }
    }

    if (recursive == false)
    {
        list_contents(path, option);
    }
    else if (recursive == true)
    {
        bool found = false;
        list_contents_recursively(path, option, &found);
    }
}

is_SF_file is_SF(char *path)
{
    is_SF_file result;
    result.sections = NULL;
    result.check = 0;
    result.version = -1;
    result.nr_sections = -1;

    int fd = open(path, O_RDONLY);
    if (fd == -1)
    {
        ERR_MSG("error when opening file");
        close(fd);
        exit(6);
    }

    // parse magic
    char magic[5];
    ssize_t read_magic = read(fd, magic, sizeof(magic) - 1);
    if (read_magic == -1)
    {
        ERR_MSG("error when reading magic");
        close(fd);
        exit(7);
    }
    magic[read_magic] = '\0';
    bool magic_condition = false;
    if (strcmp(magic, MAGIC) == 0)
    {
        magic_condition = true;
    }

    if (lseek(fd, HEADER_SIZE, SEEK_CUR) == -1)
    {
        ERR_MSG("error lseek: unable to reposition the pointer");
        close(fd);
        exit(8);
    }

    // parse version
    int version = 0;
    ssize_t read_version = read(fd, &version, sizeof(version));
    if (read_version == -1)
    {
        ERR_MSG("error when reading version");
        close(fd);
        exit(9);
    }

    bool version_condition = false;
    if (version >= 46 && version <= 74)
        version_condition = true;
    result.version = version;

    // parse nr_sections
    char nr_sections = 0;
    ssize_t read_nr_sections = read(fd, &nr_sections, sizeof(nr_sections));
    if (read_nr_sections == -1)
    {
        ERR_MSG("error when reading no of sections");
        close(fd);
        exit(10);
    }
    bool nr_sections_condition = false;
    if (nr_sections == 2 || (nr_sections >= 6 && nr_sections <= 19))
    {
        nr_sections_condition = true;
    }
    result.nr_sections = nr_sections;

    // get each section
    result.sections = malloc(nr_sections * sizeof(section_file));
    if (result.sections == NULL)
    {
        perror("error dynamic memory allocation");
        close(fd);
        exit(11);
    }

    int section_type_values[] = {16, 45, 69, 54, 64, 70, 25};
    bool section_type_full_condition = true;
    if (nr_sections_condition == true)
    {
        if (result.sections == NULL)
        {
            perror("error: dynamic allocation section_file failed");
            free(result.sections);
            close(fd);
            exit(11);
        }

        int reposition_pointer = 11;
        for (int i = 0; i < nr_sections; i++)
        {
            result.sections[i].sect_type = 0;
            result.sections[i].sect_offset = 0;
            result.sections[i].sect_size = 0;
            result.sections[i].nr_lines = 0;
            int section_type_count = 0;
            char section_name[19] = {0};
            if (lseek(fd, reposition_pointer, SEEK_SET) == -1)
            {
                perror("error seeking the pointer to the current position");
                free(result.sections);
                close(fd);
                exit(94);
            }
            ssize_t read_section_name = read(fd, section_name, sizeof(section_name) - 1);
            if (read_section_name == -1)
            {
                ERR_MSG("error when reading section_name");
                free(result.sections);
                close(fd);
                exit(12);
            }
            section_name[read_section_name] = '\0';
            strncpy(result.sections[i].sect_name, section_name, read_section_name);

            int section_type = 0;
            ssize_t read_section_type = read(fd, &section_type, sizeof(int));
            if (read_section_type == -1)
            {
                ERR_MSG("error when reading section_type");
                close(fd);
                free(result.sections);
                exit(13);
            }
            for (int j = 0; j < 7; j++)
            {
                if (section_type == section_type_values[j])
                {
                    section_type_count++;
                }
            }
            if (section_type_count == 0)
            {
                section_type_full_condition = false;
                break;
            }
            result.sections[i].sect_type = section_type;

            int section_offset = 0;
            ssize_t read_section_offset = read(fd, &section_offset, sizeof(section_offset));
            if (read_section_offset == -1)
            {
                ERR_MSG("error when reading section_offset");
                exit(14);
            }
            result.sections[i].sect_offset = section_offset;

            int section_size = 0;
            ssize_t read_section_size = read(fd, &section_size, sizeof(section_size));
            if (read_section_size == -1)
            {
                ERR_MSG("error when reading section_size");
                free(result.sections);
                close(fd);
                exit(15);
            }
            result.sections[i].sect_size = section_size;
            reposition_pointer = lseek(fd, 0, SEEK_CUR);
            if (reposition_pointer == -1)
            {
                ERR_MSG("error lseek: unable to get current file pointer position");
                free(result.sections);
                close(fd);
                exit(16);
            }

            if (lseek(fd, section_offset, SEEK_SET) == -1)
            {
                ERR_MSG("ERROR lseek: unable to get current file pointer position");
                free(result.sections);
                close(fd);
                exit(17);
            }
            char *full_section = malloc((section_size + 1) * sizeof(char));
            ssize_t read_full_section = read(fd, full_section, section_size);
            if (read_full_section == -1)
            {
                ERR_MSG("error reading full section");
                free(result.sections);
                free(full_section);
                close(fd);
                exit(40);
            }

            int count_new_line = 0;
            for (int j = 0; j < section_size; j++)
            {
                if (full_section[j] == '\n')
                {
                    count_new_line++;
                }
            }
            result.sections[i].nr_lines = count_new_line + 1;
            free(full_section);
        }
    }

    if (magic_condition == false || version_condition == false || nr_sections_condition == false || section_type_full_condition == false)
    {
        if (magic_condition == false)
            result.check = -1;
        else if (version_condition == false)
            result.check = -2;
        else if (nr_sections_condition == false)
            result.check = -3;
        else if (section_type_full_condition == false)
            result.check = -4;
    }

    return result;
}

void parse_section_files(char *path)
{
    is_SF_file all_sections = is_SF(path);

    if (all_sections.check == -1 || all_sections.check == -2 || all_sections.check == -3 || all_sections.check == -4)
    {
        printf("ERROR\nwrong ");
        bool print = false;

        if (all_sections.check == -1)
        {
            printf("magic");
            print = true;
        }

        if (all_sections.check == -2)
        {
            if (print == true)
            {
                printf("|");
                print = false;
            }
            printf("version");
            print = true;
        }

        if (all_sections.check == -3)
        {
            if (print == true)
            {
                printf("|");
                print = false;
            }
            printf("sect_nr");
            print = true;
        }

        if (all_sections.check == -4)
        {
            if (print == true)
            {
                printf("|");
                print = false;
            }
            printf("sect_types");
        }

        printf("\n");
    }
    else if (all_sections.check == 0)
    {
        printf("SUCCESS\n");
        printf("version=%d\n", all_sections.version);
        printf("nr_sections=%d\n", all_sections.nr_sections);
        for (int i = 0; i < all_sections.nr_sections; i++)
        {
            printf("section%d: %s %d %d", i + 1, all_sections.sections[i].sect_name, all_sections.sections[i].sect_type, all_sections.sections[i].sect_size);
            printf("\n");
        }
    }
    free(all_sections.sections);
}

void process_parse_command(int argc, char **argv)
{
    char path[PATH_LEN];
    for (int i = 1; i < argc; i++)
    {
        if (strncmp(argv[i], "path", 4) == 0)
        {
            strcpy(path, argv[i] + 5);
            path[PATH_LEN] = '\0';
        }
    }
    parse_section_files(path);
}

void reverse_string(char *s)
{
    int length = strlen(s);
    for (int i = 0; i < length / 2; i++)
    {
        char aux = s[i];
        s[i] = s[length - i - 1];
        s[length - i - 1] = aux;
    }
}

void extract_section_lines(char *path, int section, long long line)
{
    int fd = open(path, O_RDONLY);
    if (fd == -1)
    {
        ERR_MSG("error when opening file");
        exit(19);
    }

    if (lseek(fd, 10, SEEK_SET) == -1)
    {
        ERR_MSG("error lseek: unable to reposition the pointer --no of sections");
        close(fd);
        exit(21);
    }

    int no_of_sections = 0;
    ssize_t read_nr_sections = read(fd, &no_of_sections, 1);
    if (read_nr_sections == -1)
    {
        ERR_MSG("error when reading no_of_sections");
        close(fd);
        exit(21);
    }

    int offset = 0;
    int size = 0;
    if (section == 1)
    {
        if (lseek(fd, 33, SEEK_SET) == -1)
        {
            ERR_MSG("error lseek: unable to reposition the pointer --first offset");
            close(fd);
            exit(18);
        }
    }
    else
    {
        if (lseek(fd, 30 * (section - 1) + 33, SEEK_SET) == -1)
        {
            ERR_MSG("error lseek: unable to reposition the pointer --loop");
            close(fd);
            exit(21);
        }
    }

    ssize_t read_offset = read(fd, &offset, sizeof(offset));
    if (read_offset == -1)
    {
        ERR_MSG("error when reading offset file");
        close(fd);
        exit(22);
    }

    ssize_t read_size = read(fd, &size, sizeof(size));
    if (read_size == -1)
    {
        ERR_MSG("error when reading file size");
        close(fd);
        exit(30);
    }

    if (lseek(fd, offset, SEEK_SET) == -1)
    {
        ERR_MSG("error lseek: unable to reposition the pointer");
        close(fd);
        exit(23);
    }

    char *section_content = malloc(size * sizeof(char));
    if (section_content == NULL)
    {
        ERR_MSG("error allocating memory for section content");
        free(section_content);
        close(fd);
        exit(32);
    }

    ssize_t read_section = read(fd, section_content, size);
    if (read_section == -1)
    {
        ERR_MSG("error reading section content");
        free(section_content);
        close(fd);
        exit(24);
    }

    int current_line = 1;
    int start_offset = offset;
    int end_offset = 0;
    bool found = false;

    for (int i = 0; i < read_section; i++)
    {
        if (section_content[i] == '\r')
        {
            if (current_line == line)
            {
                end_offset = offset + i;
                found = true;
                break;
            }
            else
            {
                start_offset = offset + i + 2;
                current_line++;
            }
        }
        if (i == read_section - 1)
        {
            end_offset = offset + i + 2;
            if (current_line == line)
                found = true;
        }
    }

    if (!found || current_line < line)
    {
        perror("error line not found or section empty");
        free(section_content);
        close(fd);
        exit(1);
    }

    int line_length = end_offset - start_offset;
    char *line_content = malloc((line_length + 1) * sizeof(char));
    if (line_content == NULL)
    {
        ERR_MSG("error allocating memory for line content");
        free(line_content);
        free(section_content);
        close(fd);
        exit(25);
    }

    if (lseek(fd, start_offset, SEEK_SET) == -1)
    {
        ERR_MSG("error seeking the pointer to the start of the line");
        free(line_content);
        free(section_content);
        close(fd);
        exit(26);
    }
    ssize_t read_line_content = read(fd, line_content, line_length);
    if (read_line_content == -1)
    {
        ERR_MSG("error reading the line content");
        free(line_content);
        free(section_content);
        close(fd);
        exit(27);
    }
    line_content[line_length] = '\0';
    reverse_string(line_content);

    printf("SUCCESS\n");
    printf("%s\n", line_content);

    free(line_content);
    free(section_content);
    close(fd);
}

void process_extract_command(int argc, char **argv)
{
    char path[PATH_LEN];
    long long nr_sect = 0, line = 0;
    for (int i = 1; i < argc; i++)
    {
        if (strncmp(argv[i], "path", 4) == 0)
        {
            strcpy(path, argv[i] + 5);
            path[PATH_LEN] = '\0';
        }
        else if (strncmp(argv[i], "section", 7) == 0)
        {
            nr_sect = atoi(argv[i] + 8);
        }
        else if (strncmp(argv[i], "line", 4) == 0)
        {
            line = atoi(argv[i] + 5);
        }
    }
    extract_section_lines(path, nr_sect, line);
}

void findall_section_files(char *path, bool *success)
{
    DIR *dir;
    struct stat inode;
    struct dirent *entry;

    dir = opendir(path);
    if (dir == NULL)
    {
        perror("ERROR");
        ERR_MSG("invalid directory path"); // error when opening directory
        exit(32);
    }
    while ((entry = readdir(dir)) != NULL)
    {
        char contentPath[PATH_LEN] = " ";
        snprintf(contentPath, PATH_LEN, "%s/%s", path, entry->d_name); // build the complete path to the element in the directory

        if ((stat(contentPath, &inode)) == -1)
        {
            perror("stat in listing");
            closedir(dir);
            exit(3);
        }

        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        {
            if (S_ISDIR(inode.st_mode))
            {
                findall_section_files(contentPath, success);
            }
            else if (S_ISREG(inode.st_mode))
            {
                is_SF_file result = is_SF(contentPath);
                if (result.check < 0)
                {
                    free(result.sections);
                    continue;
                }
                else
                {
                    if (result.nr_sections >= 1)
                    {
                        bool found = false;
                        for (int j = 0; j < result.nr_sections && !found; j++)
                        {
                            if (result.sections[j].nr_lines == 14)
                            {
                                if (*success == false)
                                {
                                    printf("SUCCESS\n");
                                    *success = true;
                                }
                                printf("%s\n", contentPath);
                                found = true;
                            }
                        }
                    }
                }
                free(result.sections);
            }
        }
    }
    closedir(dir);
}

void process_findall_command(int argc, char **argv)
{
    char path[PATH_LEN] = " ";
    for (int i = 2; i < argc; i++)
    {
        if (strncmp(argv[i], "path", 4) == 0)
        {
            strcpy(path, argv[i] + 5);
            path[PATH_LEN - 1] = '\0';
        }
    }
    bool success = false;
    findall_section_files(path, &success);
}

int main(int argc, char **argv)
{

    if (argc < 2)
    {
        printf("USAGE: %s dir_name\n", argv[0]);
        exit(1);
    }

    if (argc >= 2)
    {
        for (int j = 0; j < argc; j++)
        {
            if (strcmp(argv[j], "variant") == 0)
            {
                printf("62540");
            }
            else if (strcmp(argv[j], "list") == 0)
            {
                process_list_command(argc, argv);
            }
            else if (strcmp(argv[j], "parse") == 0)
            {
                process_parse_command(argc, argv);
            }
            else if (strcmp(argv[j], "extract") == 0)
            {
                process_extract_command(argc, argv);
            }
            else if (strcmp(argv[j], "findall") == 0)
            {
                process_findall_command(argc, argv);
            }
        }
    }
    return 0;
}