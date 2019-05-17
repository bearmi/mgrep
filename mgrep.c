#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


#define debug 0
#define LINE_SIZE  2048

char no_color[] = "\x1B[0m";
char *colors[] ={"\x1B[32m", "\x1B[33m", "\x1B[34m", "\x1B[35m", "\x1B[36m", "\x1B[31m"};
int color_num = 6;

#define KWHT  "\x1B[37m"

enum{
	ACTION_UNKNOWN,
	ACTION_OR,
	ACTION_AND,
	ACTION_XOR,
	ACTION_FOLLOW
};



struct node{
	char * str;
	int action;
	int follow_cnt;
	int full_match;
	struct node * next;
	char * color;
	};

typedef struct node NODE;

enum sts {
	STS_UNKNOWN,
	STS_TRUE,
	STS_TO_PRINT,
	STS_TO_PRINT_FOLLOW
	};


NODE * filters = NULL;

static int follow_cnt;
int color_text = 1;

int mgrep_file(char * filename);

int is_full_word(char *buf, char *t, int len) {
	char k;

	if (t > buf) {
		k = *(t - 1);
		if (k >= '0' && k <= '9') return 0;
		if (k >= 'A' && k <= 'Z') return 0;
		if (k >= 'a' && k <= 'z') return 0;
		if (k == '_' ) return 0;
	}

	if ((t + len) < (buf + strlen(buf))) {
                k = *(t + len);
                if (k >= '0' && k <= '9') return 0;
                if (k >= 'A' && k <= 'Z') return 0;
                if (k >= 'a' && k <= 'z') return 0;
                if (k == '_' ) return 0;
        }

	return 1;
}

enum sts handle_node(char * buf, NODE *n, enum sts status)
{
	int matched = 0;
	char *t;

	if (debug)printf("\r\nbear2 %s vs %s, sts:%d act:%d\r\n", buf, n->str, status, n->action);

	if (status == STS_TO_PRINT || status == STS_TO_PRINT_FOLLOW)
		return status;
	else if (status == STS_TRUE && n->action == ACTION_FOLLOW) {
		follow_cnt = n->follow_cnt;
		return STS_TO_PRINT_FOLLOW;
	}

	if (t = strcasestr(buf, n->str))
		matched = 1;
	if (n->full_match && matched) {
		if (! is_full_word(buf, t, strlen(n->str)))
			matched = 0;
	}


	if (status == STS_UNKNOWN && (n->action == ACTION_XOR || n->action == ACTION_AND))
		return status;

	if (n->action == ACTION_OR) {
		if (status == STS_TRUE)
			return STS_TO_PRINT;
		else if(status == STS_UNKNOWN && matched == 1)
			return STS_TRUE;
		else if(status == STS_TO_PRINT_FOLLOW)
			return STS_TO_PRINT_FOLLOW;
		else
			return STS_UNKNOWN;
	}

	if (matched && n->action == ACTION_AND)
		return STS_TRUE;
	else if (!matched && n->action == ACTION_XOR)
		return STS_TRUE;
	else
		return STS_UNKNOWN;
}


int main(int argc, char *argv[])
{

	int i;
	int j;

	if (argc < 2) {
		printf("help: %s -nc <keywords> <filename>\r\n", argv[0]);
		printf("keywords: 'abc def' aa&bb cc^dd ee+4 <fghi>\r\n");
		return 0;
	}

	printf("[command line :] %s %s", argv[0], argv[1]);
	for (i=2; i<argc; i++)
		printf(" %s", argv[i]);
	printf("\r\n\r\n");

	char *keyword_str = argv[1];
	int file_name_index = 2;
	if (strcmp(argv[1], "-nc") == 0) {
		color_text = 0;
		keyword_str = argv[2];
		file_name_index = 3;
	}
	NODE **t = NULL, *k = NULL;
	char * p;

	// replace space by `
	j = 0;
	for (p=keyword_str; *p != '\0'; p++) {
		if (*p == '\'') {
			j = !j;
			continue;
		}
		else if ( j && *p == ' ')
			*p = '`';
	}

	j = 0;
	t = &filters;
	p = keyword_str;
	while (p = strtok(p, " ")) {
		*t = (NODE *)malloc(sizeof(NODE));
		if (*t == NULL) return(1);
		(*t)->str = p;
		(*t)->action = ACTION_OR;
		(*t)->full_match = 0;
                (*t)->color = colors[j%color_num];
                j++;
		(*t)->next = NULL;
		t = &((*t)->next);

		do {

			static int follow_cnt = 0;
			i = ACTION_UNKNOWN;

			while (*p != '\0') {
				if (*p == '&')
					i = ACTION_AND;
				else if (*p == '^')
					i = ACTION_XOR;
				else if (*p == '+' && *(p+1) > '0' && *(p+1) <= '9') {
					i = ACTION_FOLLOW;
					follow_cnt = *(p+1) - '0';
					follow_cnt ++;
				} 
				if (i != ACTION_UNKNOWN)
					break;
				p++;
			}

			if (*p == '\0') break;
			*p = '\0';
			p++;
			if (*p == '\0') break;

			*t = (NODE *)malloc(sizeof(NODE));
			if (*t == NULL) return(1);
			(*t)->str = p;
			(*t)->action = i;
			(*t)->follow_cnt = follow_cnt;
			(*t)->full_match = 0;
			(*t)->color = colors[j%color_num];
			j++;
			(*t)->next = NULL;
			t = &((*t)->next);
		} while( *p != '\0');
		p = NULL;
	}


	k=filters;
	while(k) {
		p = k->str;

		// post-replace ` by space
		if (*p == '\'') {
			*(k->str + strlen(k->str) -1) = '\0';
			k->str++;
			
			for (p = k->str; *p != '\0'; p++)
				if (*p == '`') *p = ' ';
		}

		// post-handle full word match
		if (*p == '<' && *(p + strlen(p) -1) == '>') {
			*(p + strlen(p) -1) = '\0';
			k->str ++;
			k->full_match = 1;
		}

		k = k->next;
	}


#if debug
	printf("unknown action = %d %d\r\n", ACTION_UNKNOWN, argc);

 	k=filters;
	 	while(k) {
 		printf("keywords [%d] -%s%s%s-%d-%d\r\n", k->action, k->color, k->str, no_color, k->follow_cnt, k->full_match);
	k = k->next;
	}
#endif

	if (argc > file_name_index)
            for (; file_name_index<argc; file_name_index++) {
		mgrep_file(argv[file_name_index]);
	}
        else
		mgrep_file("");
}
	

char * print_len_text(char *buf, int *buf_len, char * str, int len) {
	if (len == 0) len = strlen(str);
	int i = snprintf(buf, *buf_len, "%.*s", len, str);
	*buf_len -= i;
	return buf + i;
}
 

void colorful_text(char *in, char * out)
{
	NODE *k = filters;
	char *t = in;
	char *find, *find_loc = NULL;
	NODE *find_nod = NULL;
	int buf_len = LINE_SIZE;
	char * buf = out;

	while(t != NULL && *t != '\0') {
		char *m = NULL;
		find_nod = NULL;
		find_loc = NULL;
		k = filters;
		while(k) {
			find = strcasestr(t, k->str);
			if (find) {
				if (find_loc == NULL || find < find_loc || (find == find_loc && strlen(k->str) > strlen(find_nod->str))) {
					find_loc = find;
					find_nod = k;
				}
			}
//	printf("keywords [%d] -%s-%d-\r\n", k->action, k->str, k->follow_cnt);
			k = k->next;
		}


		if(find_nod) {
			if ( find_loc != t) buf = print_len_text(buf, &buf_len, t, find_loc - t);
			buf = print_len_text(buf, &buf_len, find_nod->color, 0);
			buf = print_len_text(buf, &buf_len, find_loc, strlen(find_nod->str));
			buf = print_len_text(buf, &buf_len, no_color, 0);
			t = find_loc + strlen(find_nod->str);
		}
		else {
			buf = print_len_text(buf, &buf_len, t, 0);
			return;
		}
	}
	return;
}

int mgrep_file(char * filename)
{
        FILE *fp_input = NULL;
	char buf[LINE_SIZE];
	char c_text[LINE_SIZE];
	NODE *k = NULL;
	enum sts status;
	struct stat s_buf;

	if (strlen(filename) != 0) {
	stat(filename, &s_buf);

	if (S_ISDIR(s_buf.st_mode))
		return 0;

	if (! S_ISREG(s_buf.st_mode))
		return 0;

//	if (s_buf.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
//		return 0;

        if ((fp_input = fopen(filename, "r")) == NULL) {
                printf("failed to open input file: %s\r\n", filename);
                return 0;
        }

	}
	else
		fp_input = stdin;	

	while (!feof(fp_input)) {
		if (!fgets(buf, LINE_SIZE, fp_input))
			return 0;
		if (debug) printf("bear3 %s follow_cnt=%d\r\n", buf, follow_cnt);

		if (status == STS_TO_PRINT_FOLLOW) {
			follow_cnt--;
			if (follow_cnt <= 0)
				status = STS_UNKNOWN;
		}
		
		if (status != STS_TO_PRINT_FOLLOW) 
		{
			k=filters;
			status = STS_UNKNOWN;
			while (k) {
				status = handle_node(buf, k, status);
				k = k->next;
			}
		}
		if (status == STS_TO_PRINT || status == STS_TRUE || status == STS_TO_PRINT_FOLLOW) {
			if (color_text) colorful_text(buf, c_text);
			printf("[%s] %s", filename, (color_text? c_text : buf));
		}
	}

	fclose(fp_input);

	return 0;
}
