#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>

#define VERSION_NUMBER "1.0"

static struct option longOptions[] =
        {
          {"error-file",          required_argument, 0, 'e'},
          {"help",                no_argument,       0, 'h'},
          {"pop3-addr",           required_argument, 0, 'l'},
          {"manager-addr",        required_argument, 0, 'L'},
          {"replace-msg",         required_argument, 0, 'm'},
          {"media-type-censure",  required_argument, 0, 'M'},
          {"manager-port",        required_argument, 0, 'o'},
          {"local-port",          required_argument, 0, 'p'},
          {"origin-port",         required_argument, 0, 'P'},
          {"cmd",                 required_argument, 0, 't'},
          {"version",             no_argument,       0, 'v'},
          {0, 0, 0, 0}
        };

int main (int argc, char ** argv)
{
  
  //checkGreatherOrEqualThan(2, argc, "pop3filter -Invalid Arguments");
  int c;

  while (true)
    {
      int optionIndex = 0;
      c = getopt_long (argc, argv, "e:hl:L:m:M:o:p:P:t:v", longOptions, &optionIndex);
      if (c == -1)
        break;

      switch (c)
        {
  /*      case 0:
          if (longOptions[optionIndex].flag != 0)
            break;
          printf ("option %s", longOptions[optionIndex].name);
          if (optarg)
            printf (" with arg %s", optarg);
          printf ("\n");
          break;
*/
        case 'e':
         printf ("option -e with value `%s'\n", optarg);
          break;

        case 'h':
          puts ("option -h\n");
          break;

        case 'l':
          printf ("option -l with value `%s'\n", optarg);
          break;

        case 'L':
          printf ("option -L with value `%s'\n", optarg);
          break;

        case 'm':
          printf ("option -m with value `%s'\n", optarg);
          break;

        case 'M':
          printf ("option -M with value `%s'\n", optarg);
          break;

        case 'o':
          printf ("option -o with value `%s'\n", optarg);
          break;

        case 'p':
          printf ("option -p with value `%s'\n", optarg);
          break;

        case 'P':
          printf ("option -P with value `%s'\n", optarg);
          break;

        case 't':
          printf ("option -t with value `%s'\n", optarg);
          break;

       /* case '?':
           getopt_long already printed an error message. 
          break;
*/
        case 'v':
          puts ("option -v\n");
          break;

        default:
          //fail("pop3filter -Invalid Options");
          abort ();
        }
    }
  //checkAreEquals(arg - optind, 1, "pop3filter -Invalid Arguments")
  printf("Origin Server Address: %s\n",argv[optind] );

 return 0;
}