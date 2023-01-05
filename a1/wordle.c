#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <unistd.h>
#include <csse2310a1.h>

// maximum number of characters in the guess inputed by the user
#define MAX_CHARACTER 60 
// lower case char minus this number to get its position in the alphabet 
#define TO_POSITION 97

/* struct of variables connected to the state of the game */
typedef struct Parameters {
    int wordLength;
    int maxGuesses;
    char* dictionary;
    char* correctWord;
} Parameters;

void exit_wordle(int exitStatus, char* dict);
void process_args(Parameters* parameters, int argc, char** argv);
void set_param(Parameters* parameters);
bool valid_guess(char* guessWord, int wordLength);
char** parse_dict(char* dictionaryFile, int* dictSize);
bool word_in_dict(char* guessWord, char** dictionary, int dictSize);
void guess_cmp(char* guessWord, Parameters parameters);

int main(int argc, char** argv) {
    Parameters parameters;
    process_args(&parameters, argc, argv);
    parameters.correctWord = get_random_word(parameters.wordLength);
    char guessWord[MAX_CHARACTER]; 
    bool guessCorrect = false;
    char* result;
    int dictSize = 0;
    char** dictionary = parse_dict(parameters.dictionary, &dictSize);
    printf("Welcome to Wordle!\n");
    // begin player input loop
    while (parameters.maxGuesses) {
	if (parameters.maxGuesses == 1) {
	    printf("Enter a %d letter word (last attempt):\n", 
		    parameters.wordLength);
	} else {
	    printf("Enter a %d letter word (%d attempts remaining):\n", 
	    	    parameters.wordLength, parameters.maxGuesses);
	}
	result = fgets(guessWord, MAX_CHARACTER, stdin);
	if (result == NULL) {
	    break;
	}
	// replaces the new line character with '\0' character
	guessWord[strcspn(guessWord, "\n")] = 0;
	// breaks from loop if guess word equals the answer word
	if (!strcasecmp(guessWord, parameters.correctWord)) {
	    guessCorrect = true;
	    break;
	}
	if (valid_guess(guessWord, parameters.wordLength)) {
	    if (word_in_dict(guessWord, dictionary, dictSize)) {
		parameters.maxGuesses--;
		guess_cmp(guessWord, parameters);
	    } else {
		printf("Word not found in the dictionary - try again.\n");
	    }
	}
    } 
    for (int i = 0; i < dictSize; i++) {
	free(dictionary[i]);
    }
    free(dictionary);
    if (result == NULL || !guessCorrect) {
	exit_wordle(3, parameters.correctWord);
    } else if (guessCorrect) {
	printf("Correct!\n");
    }
    free(parameters.correctWord);
    return 0;
}

/* guess_cmp()
 * -------------
 * Compares the guess word to the correct word and displays similaries
 * between the two. Matching behaviour of whether the right letter is present
 * and it's position determine what is printed to stdout.
 *
 * guessWord: The players guess word 
 *
 * parameters: the games state
 */
void guess_cmp(char* guessWord, Parameters parameters) {
    int wordLength = parameters.wordLength;
    char* correctWord = parameters.correctWord;
    char* matchReporting = malloc(sizeof(char) * (wordLength + 1));
    memset(matchReporting, 0, sizeof(char) * wordLength);

    // list of numbers indexed by its position in the alphabet, each number
    // signifies the amount of letters present in the correct word.
    int letterCount[26], letterPos;
    memset(letterCount, 0, sizeof(int) * 26);

    // check for letters in same position
    for (int i = 0; i < wordLength; i++) {
	letterPos = tolower(correctWord[i]) - TO_POSITION;	
	letterCount[letterPos]++;
    }

    // check for letters in the same position
    for (int i = 0; i < wordLength; i++) {
	if (tolower(guessWord[i]) == tolower(correctWord[i])) {
	    matchReporting[i] = toupper(guessWord[i]);
	    letterPos = tolower(guessWord[i]) - TO_POSITION;
	    letterCount[letterPos]--;
	}
    }
    // lower cased letter of the guess word
    char guessLetter;

    // go through guess word letters
    for (int guessIdx = 0; guessIdx < wordLength; guessIdx++) {
	guessLetter = tolower(guessWord[guessIdx]);
	// check if the letter is in the same position
	if (matchReporting[guessIdx] == 0) {
	    // check whether or not if another letter is present
	    letterPos = guessLetter - TO_POSITION;
	    if (letterCount[letterPos] > 0) {
		matchReporting[guessIdx] = guessLetter;
		letterCount[letterPos]--;
	    }
	}
    }
    for (int i = 0; i < wordLength; i++) {
	if (matchReporting[i] == 0) {
	    printf("%c", '-');
	} else {
	    printf("%c", matchReporting[i]);
	}
    }
    printf("\n");
    free(matchReporting);
}

/* word_in_dict()
 * --------------
 * Checks to see if the guess word exists in the dictionary list.
 *
 * guessWord: the guessed word to be matched
 *
 *
 * dictionary: the dictionary list to be parsed
 *
 * dictSize: the amount of words in the dictionary
 */
bool word_in_dict(char* guessWord, char** dictionary, int dictSize) {
    for (int i = 0; i < dictSize; i++) {
	if (!strcasecmp(dictionary[i], guessWord)) {
	    return true;
	}
    }
    return false;
}

/* parse_dict()
 * ------------
 * Parses through the dictionary file given and then returns a list of the
 * words in the dictionary. Updates dictionary size pointer to match 
 * amount of words in dictionary.
 *
 * dictionaryFile: the location of the dictionary file to be opened
 *
 * dictSize: the address of an integer whose value will be changed to the 
 *           dictionary list size
 *           
 * return: list of the dictionary words
 */
char** parse_dict(char* dictionaryFile, int* dictSize) {
    char** dictionary = malloc(sizeof(char*));
    int size = 0;
    FILE* dictFile = fopen(dictionaryFile, "r");
    char dictWord[MAX_CHARACTER];

    while (fgets(dictWord, MAX_CHARACTER, dictFile) != NULL) {
	dictWord[strcspn(dictWord, "\n")] = 0;
	dictionary = realloc(dictionary, sizeof(char*) * (size + 1));
	dictionary[size++] = strdup(dictWord);
    }
    *dictSize = size;
    fclose(dictFile);
    return dictionary;
}

/* valid_guess()
 * -------------
 * Checks if the guess word is of the correct length and only 
 * contains letters. 
 *
 * guessWord: the word to be validated
 *
 * wordLength: the length the guess word must be equal to
 */
bool valid_guess(char* guessWord, int wordLength) {
    int length = strlen(guessWord);
    if (length != wordLength) {
	printf("Words must be %d letters long - try again.\n", wordLength);
	return false;
    }
    for (int i = 0; i < wordLength; i++) {
	if (!isalpha(guessWord[i])) {
	    printf("Words must contain only letters - try again.\n");
	    return false;
	}
    }
    return true;
}

/* process_args()
 * --------------
 * Processes the command line arguments updating the struct parameters 
 * accordingly. An invalid command line argument will exit the program after
 * printing an error message corresponding to exit_wordle().
 *
 * parameters: struct Parameters address to modify according to command line
 *             arguments
 *
 * argc: number of arguments
 *
 * argv: arguments
 */
void process_args(Parameters* parameters, int argc, char** argv) {
    set_param(parameters);
    // pass through first argument
    argc--;
    argv++;
    if (argc > 5) {
	exit_wordle(1, NULL);
    }
    char* arg; 
    char* argNum;
    bool lenPresent = false, maxPresent = false;
    // parse through arguments
    for (int i = 0; i < argc; i++) {
	arg = argv[i];
	// check if arg after "-..." is a valid number 3 - 9 and then set value
	if ((arg[0] == '-') && (i + 1 != argc)) {  
	    char* emptyPtr;
	    long value;
	    i++;
	    argNum = argv[i];
	    value = strtol(argNum, &emptyPtr, 10);
	    if (strcmp(emptyPtr, "") || (value < 3) || (value > 9)) {
		exit_wordle(1, NULL);
	    }
	    // check if arg "-..." is valid and then update struct
	    if (!(strcmp(arg, "-len")) && !lenPresent) {
		lenPresent = true;
		parameters->wordLength = value;
	    } else if (!(strcmp(arg, "-max")) && !maxPresent) {
		maxPresent = true;
		parameters->maxGuesses = value;
	    } else {
		exit_wordle(1, NULL);
	    }
	} else if ((arg[0] != '-') && (i + 1 == argc)) {
	    parameters->dictionary = arg;
	} else {
	    exit_wordle(1, NULL);
	}
    }
    FILE* dict = fopen(parameters->dictionary, "r");

    if (dict == NULL) {
	exit_wordle(2, parameters->dictionary);
    } else {
	fclose(dict);
    }
}

/* set_param()
 * -------------
 *  sets struct Parameters to default settings
 *  
 *  parameters: Address of a struct parameters
 */
void set_param(Parameters* parameters) {
    parameters->wordLength = 5;
    parameters->maxGuesses = 6;
    parameters->dictionary = "/usr/share/dict/words";
}

/* exit_wordle()
 * --------------
 * prints an error message to standard output corresponding to the exit Status
 * given, then exiting with the exitStatus.
 *
 * exitStatus: an int value of 1 - (not sure yet).
 * 
 * description: extra information for the standard error message if required
 */
void exit_wordle(int exitStatus, char* description) {
    switch (exitStatus) {
	case 1:
	    fprintf(stderr, "Usage: wordle [-len word-length] "
		    "[-max max-guesses] [dictionary]\n");
	    exit(1);
	case 2:
	    fprintf(stderr, "wordle: dictionary file \"%s\" cannot be"
		    " opened\n", description); 
	    exit(2);
	case 3:
	    fprintf(stderr, "Bad luck - the word is \"%s\".\n", description);
	    free(description);
	    exit(3);
    }
}
