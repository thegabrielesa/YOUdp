/* prototypes */
void print_list(void);
void print_progress(struct thread_element *te);
void print_help(void);
void print_credits(void);


/******************************************************************************
**                                                                           **
**                                 Methods                                   **
**                                                                           **
******************************************************************************/
void print_list(void) {
    FILE *file_list;
    char line[MAXLINE + 1], filename[MAXLINE + 1], path[MAXLINE + 1], *str;
    off_t file_size;
    double dim;

    /* build file list path */
    strcpy(path, SYSTEM_DIR);
    strcat(path, "file_list.bin");

    file_list = fopen(path, "r");
    abort_on_error(file_list == NULL, "error in fopen");

    printf("\r+--------------------------------------------------------+\n");
    /* read file row by row (each row terminates with '\n') */
    while (fgets(line, MAXLINE, file_list) != NULL) {
        /* file name and size from row */
        sscanf(line, "%s\t%ld\n", filename, &file_size);
        
        /* printing filename */
        str = filename;
        while (strlen(str) > 45) {
            printf("\r| %-45.45s%9s |\n", str, " ");
            str += 45;
        }
        printf("\r| %-45s\t", str);
        /* printing size */
        if (file_size >= 1024) {
            /* kB */
            dim = (double) file_size / 1024;
            if (dim >= 1024) {
                /* MB */
                dim /= 1024;
                if (dim >= 1024) {
                    /* GB */
                    dim /= 1024;
                    printf("%5.1f GB |\n", dim);
                } else
                    printf("%5.1f MB |\n", dim);
            } else
                printf("%5.1f kB |\n", dim);

        } else
            printf("%5ld  B |\n", file_size);
    }
    printf("\r+--------------------------------------------------------+\n");
    printf("\r>>> ");

    fflush(stdout);
    fclose(file_list);
}

void print_progress(struct thread_element *te) {
    struct thread_element *p;
    suseconds_t us;
    double rate, percentage, s, ms;
    long int minutes = 0, hours = 0, seconds = 0;
    int sleeping = 0;

    printf("+----------------------------------------------------------------"\
            "--------------+\n");
    for (p = te; p < te + NUM_THREADS; p++) {
        /* do not show thread not scheduled */
        if (p->status == EMPTY) {
            sleeping++;
            continue;
        }

        printf("| ");
        /* filename */
        if (strlen(p->filename) > 25) printf("%-.25s...\t", p->filename);
        else printf("%-28s\t", p->filename);

        switch (p->status) {
            case HU:
                /* host unreachable */
                printf("%s %9s|\n", "Request canceled: server unreachable.", 
                        " ");
                break;
            case NSF:
                /* file not found on server */
                printf("%s %2s|\n", "Download canceled: file not found on "\
                        "server.", " ");
                break;
            case FTB:
                /* file reaches max uploading size */
                printf("%s %2s|\n", "Upload canceled: exceeded max size. "\
                        "(250 MB)", " ");
                break;
            case RUN:
                /* elapsed time in usec */
                us = ((p->now).tv_sec * 1000000 + (p->now).tv_usec) - 
                    ((p->start).tv_sec * 1000000 + (p->start).tv_usec);

                /* transmission incomplete */
                if (p->cur_pkt < p->tot_pkt) {
                    /* percentage */
                    percentage = p->cur_pkt / ((double) p->tot_pkt) * 100;
                    printf("%5.2f%%\t", percentage);
                    /* elapsed time in seconds */
                    s = us / 1000000.0; 
                    /* B/sec */
                    rate = (p->cur_pkt * DIM_PAYLOAD) / s;
                    /* remaining time */
                    seconds = (long int) (((p->tot_pkt - p->cur_pkt) * 
                                DIM_PAYLOAD) / rate);
                    /* rate conversions */
                    if (rate >= 1024) {
                        /* kB/s */
                        rate /= 1024;
                        if (rate >= 1024) {
                            /* MB/s */
                            rate /= 1024;
                            printf("%4.0f MB/sec", rate);
                        } else 
                            printf("%4.0f kB/sec", rate);
                    } else
                        printf("%4.0f  B/sec", rate);
                    /* time conversions */
                    if (seconds >= 60) {
                        minutes = seconds / 60;
                        seconds -= minutes * 60;
                        if (minutes >= 60) {
                            hours = minutes / 60;
                            minutes -= hours * 60;
                        }
                    }
                    if (hours > 99)
                        printf("\t~~:~~:~~ remaining %3s", " ");
                    else
                        printf("\t%2.2ld:%2.2ld:%2.2ld remaining %3s", 
                                hours, minutes, seconds, " ");
                /* trasmission completed */
                } else {
                    /* elapsed time in sec */
                    seconds = us / 1000000;
                    /* time conversions */
                    if (seconds >= 60) {
                        minutes = seconds / 60;
                        seconds -= minutes * 60;
                        if (minutes >= 60) {
                            hours = minutes / 60;
                            minutes -= hours * 60;
                            printf("Finished in %2.2ld hrs, %2.2ld mins and "\
                                    "%2.2ld secs. %5s", hours, minutes, 
                                    seconds, " ");
                        } else {
                            printf("Finished in %2.2ld mins and %2.2ld "\
                                    "secs. %13s", minutes, seconds, " ");
                        }
                    }
                    else if (seconds >= 1) {
                        printf("Finished in %2.2ld secs. %25s", 
                                seconds, " ");
                    } else {
                        ms = us / 1000.0;
                        printf("Finished in %3.3d msecs. %23s", 
                                (int) ms, " ");
                    }
                }
                printf(" |\n");
                break;
            default:
                break;
        }
    }

    if (sleeping == NUM_THREADS)
        printf("| %25s%s%25s |\n", " ", "No running download/upload", " ");

    printf("+----------------------------------------------------------------"\
            "--------------+\n");
}

void print_help(void) {
    printf("\n%s\n", 
    "  Welcome to YOUdp 1.0! This is the help utility.\n"\
    "  If this is your first time using YOUdp, you should definitely check "\
    "out the\n  tutorial.\n\n"\
    "  This software has been created for YOU to transfer files safety using "\
    "UDP\n  protocol.\n"\
    "  Number of simultaneos trasmissions has been limited to 10; after that "\
    "YOU\n  need to wait the ending of almost one of them.\n\n"\
    "  CASE LIST:\n"\
    "  If you want to check which files are on the server, you should type "\
    "\"list\".\n  It will automatically return all files that are available "\
    "to download.\n"\
    "  CASE GET:\n"\
    "  If you want to download any file, you should type \"get file_name\".\n"\
    "  It will automatically start the download, if that file is on the "\
    "server.\n"\
    "  CASE PUT:\n"\
    "  If you want to upload any file, you should type \"put file_name\".\n"\
    "  It will automatically start the upload, if that file is on the client"\
    "\n  \"FILES\" directory\n"\
    "  CASE STAT:\n"\
    "  If you want to know status of the last 10 downloads and/or uploads, "\
    "you\n  should type \"stat\".\n"\
    "  It will show filenames and remaining time or outcoming.\n\n"\
    "  Type \"exit\" or Ctrl-D to exit\n");
}

void print_credits(void) {
    printf("\n%s\n",
    "  Thanks to Professor Francesco Lo Presti for this opportunity.\n"\
    "  Thanks to University of Rome - Tor Vergata.\n");
}
