#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#define MAX_SYMBOLS 65536
#define MAX_LETTERS 256
#define MAX_PROPERTY_LENGTH 2

typedef unsigned char t_property[MAX_PROPERTY_LENGTH+1];
static t_property properties[MAX_SYMBOLS];

#define MAX_SYMBOL_LENGTH 4

static void init_properties(void)
{
	int i;
	for(i = 0; i < MAX_SYMBOLS; i++) {
		properties[i][0] = '\0';
	}
}

static void set_property(unsigned symbol, t_property *property) 
{
	if (symbol < MAX_SYMBOLS) {
		strcpy(properties[symbol], *property);
#if 0
		fprintf(stderr, "%04x %s %s\n", 
			symbol,
			properties[symbol],
			*property);
#endif
	}
}

static void set_property_range(
	unsigned symbol_start, unsigned symbol_end, t_property *property)
{
	unsigned i;
	for(i = symbol_start; (i <= symbol_end) && (i < MAX_SYMBOLS); i++) {
		strcpy(properties[symbol_start], *property);
	}
}


static void process_UnicodeData_input(FILE *input)
{

#define STATE_RESETTING 	0
#define STATE_GETTING_SYMBOL	1
#define STATE_SKIPPING_NAME	2
#define STATE_GETTING_LETTER	3
#define STATE_GETTING_EOL	4

	int state = STATE_RESETTING;
	char symbol_digit[MAX_SYMBOL_LENGTH+1];
	int symbol_digits;
	unsigned symbol;
	t_property property;
	int property_length;
	int c;
	int line = 1;

#define next_char() \
c = getc(input); if (c < 0) break

	c = 0;
	while (c >= 0) {
		switch (state) {
		case STATE_RESETTING:
			symbol = 0;
			symbol_digits = 0;
			symbol_digit[0] = '\0';
			state = STATE_GETTING_SYMBOL;
			property[0] = '\0';
			property_length = 0;
			/* fall through */
		case STATE_GETTING_SYMBOL:
			next_char();
			if ((symbol_digits <= MAX_SYMBOL_LENGTH) &&
				(isxdigit(c))) {
				symbol_digit[symbol_digits++] = c;
			} 
			else if (c == ';') {
				symbol_digit[symbol_digits] = '\0';
				symbol = strtoul(symbol_digit, 0, 16);
				state = STATE_SKIPPING_NAME;
			} else {
				fprintf(stderr, "Bad symbol number line %d\n", line);
				state = STATE_GETTING_EOL;
			}
			break;
		case STATE_SKIPPING_NAME:
			next_char();
			if (c == ';') {
				state = STATE_GETTING_LETTER;
			}
			break;
		case STATE_GETTING_LETTER:
			next_char();
			if ((property_length < MAX_PROPERTY_LENGTH) &&
				isalpha(c)) {
				property[property_length++] = c;
#if 0
				fprintf(stderr, "%c\n", c);
#endif
			}
			else if (c == ';') {
				property[property_length] = '\0';
				set_property(symbol, &property);
				state = STATE_GETTING_EOL;
			} 
			else {
				fprintf(stderr, "Can't find letter on line %d\n", line);
				state = STATE_GETTING_EOL;
			}
			break;
		case STATE_GETTING_EOL:
			next_char();
			if (c == '\n') {
				line++;
				state = STATE_RESETTING;
			}
			break;
		}
	}
#undef STATE_RESETTING
#undef STATE_GETTING_SYMBOL
#undef STATE_SKIPPING_NAME
#undef STATE_GETTING_LETTER
#undef STATE_GETTING_EOL
}

static void create_crunched_output(char *name, FILE *output)
{
	int i;
	t_property *last_property = 0;
	fprintf(output, 
		"# This file has been automatically generated by %s\n"
		"# Do not modify\n",
		name );
	for(i = 0; i < MAX_SYMBOLS; i++) {
		if (last_property) {
			if (strcmp(*last_property, properties[i]) != 0) {
				if (last_property != &properties[i-1]) {
					fprintf(output, "-%04X", i-1);
				}
				fprintf(output, "\n");
				last_property = 0;
			}
		} 
		if (!last_property && properties[i][0] != '\0') {
			last_property = &properties[i];
			fprintf(output, "%s %04X", *last_property, i);
		}
	}
	if (last_property) {
		if (last_property != &properties[i-1]) {
			fprintf(output, "-%04X", i-1);
		}
		fprintf(output, "\n");
	}
}

static void init(void)
{
	init_properties();
}

static int crunch_UnicodeData(char *name, FILE *in, FILE *out)
{
	init();
	process_UnicodeData_input(in);
	create_crunched_output(name, out);
	return 0;
}

int main(int argc, char **argv)
{
	return crunch_UnicodeData(argv[0], stdin, stdout);
}
