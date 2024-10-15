#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/mman.h>

#define RESP_PIPE "RESP_PIPE_62540"
#define REQ_PIPE "REQ_PIPE_62540"
#define BUFFER_SIZE 512

#define MAGIC "KlxO"
#define HEADER_SIZE 2
#define VERSION 4
#define NO_OF_SECTIONS 2
#define ALIGNMENT 5120

// shared memory
unsigned int mem_size = 0;
void *shared_data;

// memory where the file was mapped
off_t file_length;
void *file_data;

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

typedef struct
{
    unsigned int logical_start;
    unsigned int logical_end;
} sections_logical;

void *read_from_memory(void *sf_file, unsigned int no_of_bytes)
{
    void *result = malloc(no_of_bytes);
    memcpy(result, sf_file, no_of_bytes);
    return result;
}

is_SF_file is_SF()
{
    void *sf_file = file_data;

    is_SF_file result;
    result.sections = NULL;
    result.check = 0;
    result.version = -1;
    result.nr_sections = -1;

    sf_file = file_data + 10;

    // parse nr_sections
    char nr_sections = 0;
    memcpy(&nr_sections, read_from_memory(sf_file, sizeof(nr_sections)), sizeof(nr_sections));
    sf_file += sizeof(nr_sections);
    result.nr_sections = nr_sections;

    // get each section
    result.sections = malloc(nr_sections * sizeof(section_file));
    if (result.sections == NULL)
    {
        perror("error dynamic memory allocation");
        exit(11);
    }

    for (int i = 0; i < nr_sections; i++)
    {
        result.sections[i].sect_type = 0;
        result.sections[i].sect_offset = 0;
        result.sections[i].sect_size = 0;
        result.sections[i].nr_lines = 0;
        char section_name[19] = {0};

        memcpy(&section_name, read_from_memory(sf_file, sizeof(section_name) - 1), sizeof(section_name) - 1);
        sf_file += sizeof(section_name) - 1;
        section_name[19] = '\0';
        strncpy(result.sections[i].sect_name, section_name, sizeof(section_name) - 1);

        int section_type = 0;
        memcpy(&section_type, read_from_memory(sf_file, sizeof(section_type)), sizeof(section_type));
        sf_file += sizeof(section_type);
        result.sections[i].sect_type = section_type;

        int section_offset = 0;
        memcpy(&section_offset, read_from_memory(sf_file, sizeof(section_offset)), sizeof(section_offset));
        sf_file += sizeof(section_offset);
        result.sections[i].sect_offset = section_offset;

        int section_size = 0;
        memcpy(&section_size, read_from_memory(sf_file, sizeof(section_size)), sizeof(section_size));
        sf_file += sizeof(section_size);
        result.sections[i].sect_size = section_size;
    }

    return result;
}

void ping_pong_request(int fd_resp)
{
    write(fd_resp, "PING#", 5);
    unsigned int variant = 62540;
    write(fd_resp, &variant, sizeof(variant));
    write(fd_resp, "PONG#", 5);
}

void shm_creation_request(int fd_resp, int mem_size)
{
    int shm_fd;
    char shm_name[256] = "/yEV6my";
    shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0664);

    if (shm_fd < 0)
    {
        write(fd_resp, "CREATE_SHM#ERROR#", 17);
        perror("error when creating the shared memory");
        close(fd_resp);
        exit(-1);
    }

    if (ftruncate(shm_fd, mem_size) < 0)
    {
        write(fd_resp, "CREATE_SHM#ERROR#", 17);
        perror("error when truncating shared memory");
        close(shm_fd);
        close(fd_resp);
        exit(-1);
    }

    shared_data = mmap(NULL, mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_data == MAP_FAILED)
    {
        perror("error when mapping shared memory file");
        write(fd_resp, "CREATE_SHM#ERROR#", 17);
        close(shm_fd);
        close(fd_resp);
        exit(-1);
    }

    write(fd_resp, "CREATE_SHM#SUCCESS#", 19);
}

void write_to_shm_request(int fd_resp, unsigned int offset, unsigned int value)
{
    if (offset <= mem_size && offset + sizeof(value) <= mem_size)
    {
        *((unsigned int *)(shared_data + offset)) = value;
        write(fd_resp, "WRITE_TO_SHM#SUCCESS#", strlen("WRITE_TO_SHM#SUCCESS#"));
    }
    else
    {
        write(fd_resp, "WRITE_TO_SHM#ERROR#", strlen("WRITE_TO_SHM#ERROR#"));
    }
}

void memory_map_file_request(int fd_resp, char *file_name)
{
    int fd;

    fd = open(file_name, O_RDONLY);
    if (fd < 0)
    {
        write(fd_resp, "MAP_FILE#ERROR#", strlen("MAP_FILE#ERROR#"));
        perror("error when opening the file for reading");
        close(fd);
        return;
    }

    file_length = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    if ((file_data = mmap(NULL, file_length, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED)
    {
        write(fd_resp, "MAP_FILE#ERROR#", strlen("MAP_FILE#ERROR#"));
        perror("error when mapping");
        close(fd);
        return;
    }

    close(fd);
    write(fd_resp, "MAP_FILE#SUCCESS#", strlen("MAP_FILE#SUCCESS#"));
}

void read_from_file_offset_request(int fd_resp, unsigned int offset, unsigned int no_of_bytes)
{
    if (shared_data == NULL || file_data == NULL)
    {
        write(fd_resp, "READ_FROM_FILE_OFFSET#ERROR#", strlen("READ_FROM_FILE_OFFSET#ERROR#"));
        return;
    }

    if (offset + no_of_bytes > file_length)
    {
        write(fd_resp, "READ_FROM_FILE_OFFSET#ERROR#", strlen("READ_FROM_FILE_OFFSET#ERROR#"));
        return;
    }

    char *buffer_data = file_data;
    char *source = (char *)buffer_data + offset;
    char *dest = (char *)shared_data;

    memcpy(dest, source, no_of_bytes);

    write(fd_resp, "READ_FROM_FILE_OFFSET#SUCCESS#", strlen("READ_FROM_FILE_OFFSET#SUCCESS#"));
}

void read_from_file_section(int fd_resp, unsigned int section_no, unsigned int offset, unsigned int no_of_bytes)
{
    is_SF_file result = is_SF();
    unsigned int nr_sections = result.nr_sections;
    unsigned int section_offset = result.sections[section_no - 1].sect_offset;
    unsigned int section_size = result.sections[section_no - 1].sect_size;

    if (section_no > nr_sections)
    {
        write(fd_resp, "READ_FROM_FILE_SECTION#ERROR#", strlen("READ_FROM_FILE_SECTION#ERROR#"));
        perror("section number unavailable");
        free(result.sections);
        return;
    }

    if (offset + no_of_bytes > section_size)
    {
        write(fd_resp, "READ_FROM_FILE_SECTION#ERROR#", strlen("READ_FROM_FILE_SECTION#ERROR#"));
        perror("offset out of bounds");
        free(result.sections);
        return;
    }

    if (offset + no_of_bytes + section_offset > file_length)
    {
        write(fd_resp, "READ_FROM_FILE_SECTION#ERROR#", strlen("READ_FROM_FILE_SECTION#ERROR#"));
        perror("offset out of bounds");
        free(result.sections);
        return;
    }

    void *buffer_data = file_data;
    char *section_data = (char *)buffer_data + section_offset + offset;
    memcpy(shared_data, section_data, no_of_bytes);

    write(fd_resp, "READ_FROM_FILE_SECTION#SUCCESS#", strlen("READ_FROM_FILE_SECTION#SUCCESS#"));
    free(result.sections);
}

// Calculate the physical offset for a given logical offset
sections_logical *calculate_physical_offset(unsigned int logical_offset, is_SF_file sf_file) {
    unsigned int logical_base = 0;
    unsigned int blocks_of_memory = 0;

    sections_logical *sections = malloc(sf_file.nr_sections * sizeof(sections_logical));
    for (int i = 0; i < sf_file.nr_sections; i++) {
        sections[i].logical_start = logical_base;
        sections[i].logical_end = logical_base + sf_file.sections[i].sect_size;
        blocks_of_memory = sections[i].logical_end / ALIGNMENT + 1;
        logical_base = ALIGNMENT * blocks_of_memory;
    }
    return sections;
}

// Handle READ_FROM_LOGICAL_SPACE_OFFSET command
void read_from_logical_memory_space_request(int fd_resp, unsigned int logical_offset, unsigned int no_of_bytes) {
    if (shared_data == NULL || file_data == NULL) {
        write(fd_resp, "READ_FROM_LOGICAL_SPACE_OFFSET#ERROR#", strlen("READ_FROM_LOGICAL_SPACE_OFFSET#ERROR#"));
        return;
    }

    is_SF_file sf_info = is_SF();
    sections_logical *sections = calculate_physical_offset(logical_offset, sf_info);

    unsigned int offset_file = 0;
    int current_section = -1;

    for (int i = 0; i < sf_info.nr_sections; i++) {
        if (sections[i].logical_start <= logical_offset && logical_offset < sections[i].logical_end) {
            current_section = i;
            offset_file = logical_offset - sections[i].logical_start + sf_info.sections[current_section].sect_offset;
            break;
        }
    }

    if (offset_file + no_of_bytes > file_length) {
        write(fd_resp, "READ_FROM_LOGICAL_SPACE_OFFSET#ERROR#", strlen("READ_FROM_LOGICAL_SPACE_OFFSET#ERROR#"));
    } else {
        memcpy(shared_data, file_data + offset_file, no_of_bytes);
        write(fd_resp, "READ_FROM_LOGICAL_SPACE_OFFSET#SUCCESS#", strlen("READ_FROM_LOGICAL_SPACE_OFFSET#SUCCESS#"));
    }

    free(sections);
    free(sf_info.sections);
}

int main(int argc, char **argv)
{
    int fd_req, fd_resp;
    char buffer;
    char command[BUFFER_SIZE];
    memset(command, 0, BUFFER_SIZE);

    if (argc != 1)
    {
        printf("Use: %s message", argv[0]);
        exit(0);
    }

    if (access(RESP_PIPE, F_OK) == -1)
    {
        if ((mkfifo(RESP_PIPE, 0600)) == -1)
        {
            printf("ERROR\ncannot create the response pipe");
            exit(EXIT_FAILURE);
        }
    }

    fd_req = open(REQ_PIPE, O_RDONLY);
    if (fd_req == -1)
    {
        printf("ERROR\ncannot open the request pipe");
        close(fd_req);
        exit(-2);
    }

    fd_resp = open(RESP_PIPE, O_WRONLY);
    if (fd_resp == -1)
    {
        printf("ERROR\ncannot open the response pipe");
        close(fd_resp);
        close(fd_req);
        exit(-3);
    }

    if (write(fd_resp, "START#", 6) == -1)
    {
        printf("error when writing to pipe ");
        close(fd_resp);
        close(fd_req);
        exit(-3);
    }

    int read_bytes = 0;
    while ((read_bytes = read(fd_req, &buffer, 1)) > 0)
    {
        if (buffer != '#')
        {
            strncat(command, &buffer, 1);
        }
        else
        {
            if ((strcmp(command, "PING")) == 0)
            {
                ping_pong_request(fd_resp);
                memset(command, 0, BUFFER_SIZE);
            }
            else if ((strcmp(command, "CREATE_SHM")) == 0)
            {
                read(fd_req, &mem_size, sizeof(mem_size));
                shm_creation_request(fd_resp, mem_size);
                memset(command, 0, BUFFER_SIZE);
            }
            else if ((strcmp(command, "WRITE_TO_SHM")) == 0)
            {
                unsigned int offset = 0;
                unsigned int value = 0;
                read(fd_req, &offset, sizeof(offset));
                read(fd_req, &value, sizeof(value));
                write_to_shm_request(fd_resp, offset, value);
                memset(command, 0, BUFFER_SIZE);
            }
            else if ((strcmp(command, "MAP_FILE")) == 0)
            {
                char file_name[BUFFER_SIZE];
                memset(file_name, 0, BUFFER_SIZE);
                int t = 1;
                while (t == 1 && (read_bytes = read(fd_req, &buffer, 1)) > 0)
                {
                    if (buffer != '#')
                    {
                        strncat(file_name, &buffer, 1);
                    }
                    else
                    {
                        t = 0;
                    }
                }
                memory_map_file_request(fd_resp, file_name);
                memset(command, 0, BUFFER_SIZE);
            }
            else if ((strcmp(command, "READ_FROM_FILE_OFFSET")) == 0)
            {
                unsigned int offset = 0;
                unsigned int no_of_bytes = 0;
                read(fd_req, &offset, sizeof(offset));
                read(fd_req, &no_of_bytes, sizeof(no_of_bytes));

                read_from_file_offset_request(fd_resp, offset, no_of_bytes);
                memset(command, 0, BUFFER_SIZE);
            }
            else if ((strcmp(command, "READ_FROM_FILE_SECTION")) == 0)
            {
                unsigned int section_no = 0, offset = 0, no_of_bytes = 0;
                read(fd_req, &section_no, sizeof(section_no));
                read(fd_req, &offset, sizeof(offset));
                read(fd_req, &no_of_bytes, sizeof(no_of_bytes));

                read_from_file_section(fd_resp, section_no, offset, no_of_bytes);
                memset(command, 0, BUFFER_SIZE);
            }
            else if ((strcmp(command, "READ_FROM_LOGICAL_SPACE_OFFSET")) == 0)
            {
                unsigned int logical_offset = 0;
                unsigned int no_of_bytes = 0;
                read(fd_req, &logical_offset, sizeof(logical_offset));
                read(fd_req, &no_of_bytes, sizeof(no_of_bytes));
                printf("%d %d", logical_offset, no_of_bytes);
                read_from_logical_memory_space_request(fd_resp, logical_offset, no_of_bytes);

                memset(command, 0, BUFFER_SIZE);
            }
            else if ((strcmp(command, "EXIT")) == 0)
            {
                close(fd_resp);
                close(fd_req);
                exit(0);
            }
        }
    }

    return 0;
}
