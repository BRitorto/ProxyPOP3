#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

#define VERSION_NUMBER "1.0"


int main (int argc, char ** argv)
{
  
  //checkGreatherOrEqualThan(2, argc, "pop3filter -Invalid Arguments");
  int c;

  while ((c = getopt(argc-1, argv, "e:hl:L:m:M:o:p:P:t:v")) != -1)
    {
      switch (c)
        {
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

        case '?':
            if (optopt == 'e')
                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            if (optopt == 'l')
                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            if (optopt == 'L')
                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            if (optopt == 'm')
                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            if (optopt == 'M')
                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            if (optopt == 'o')
                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            if (optopt == 'p')
                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            if (optopt == 'P')
                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            if (optopt == 't')
                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            else if (isprint (optopt))
                fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf (stderr,"Unknown option character `\\x%x'.\n", optopt);
            return 1;
        case 'v':
          puts ("option -v\n");
          break;

        default:
          //fail("pop3filter -Invalid Options");
          abort ();
        }
    }
  for (int index = optind; index < argc - 1; index++)
    printf ("Non-option argument %s\n", argv[index]);
  //checkAreEquals(arg - optind, 1, "pop3filter -Invalid Arguments")
  printf("Origin Server Address: %s\n",argv[optind] );

 return 0;
}
