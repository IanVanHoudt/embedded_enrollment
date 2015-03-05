/*
  Ian Van Houdt
  CS 586
  embedded_enrollment.c

  This file will use an embedded SQL routine to generate enrollment records in the student
  registry database. It will go through student records, find each student's major(s), join
  the requisite tables to find the course_ids for each major, check that prerequisites are
  already taken, then either adds that course or continues to randomly select courses from 
  that major and try to add them.

  There is the potential that an unlucky random selection of course will leave a student with
  no enrollment (each randomly selected course required a course that wasn't already taken). 
  This is easily fixed by re-running the program with that student's id as an argument.  Worst
  case scenario is that you have to run this a few time and prune the results (if a student has
  taken 5 classes every term or something like that).

  Compile as: 
  gcc -I /usr/include/postgresql -L /usr/lib/postgresql -o embedded_enrollment embedded_enrollment.c.c -lpq

*/

#include <stdio.h>
#include <stdlib.h>
#include <libpq-fe.h>
#include <string.h>

int exit_nicely(PGconn *conn, char *loc)
{
    PQfinish(conn);
    fprintf(stderr, "\n*****Whoa, had and issue (%s)! Exiting\n", loc);
    exit(1);
}


int main(int argc, char *argv[])
{
    const int DEBUG = 1;
    FILE *f = fopen("debug_output.txt", "w");
    if (f == NULL)
    {
        fprintf(stderr, "Issue opening output file\n");
        exit (1);
    }

    int input_student_id = 0;

    if (argc < 2)
        fprintf(stderr, "NOTE: You are executing with no student_id, and therefore will execute on all students\n");

    if (argc == 2)
        input_student_id = atoi(argv[1]);

    const char *conn_info;
    PGconn *conn;
    PGresult *res;

    conn_info = "host=dbclass.cs.pdx.edu user=w15db71 password=secret";
    conn = PQconnectdb(conn_info);

    if (PQstatus(conn) != CONNECTION_OK)
    {
        fprintf(stderr, "Connection to DB failed: %s\n", PQerrorMessage(conn));
        return -1;
    }

    char *student_buffer = (char *) malloc (sizeof(char) * 1024);
    sprintf(student_buffer, "select s.id from registry.student s;");    
    res = PQexec(conn, student_buffer);

    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        free(student_buffer);
        exit_nicely(conn, "Getting student records");
    }

    int NUM_STUD = PQntuples(res);

    //start from beginning to generate enrollment for all student
    int i = 1;     
    //if student_id provided at cli, only do so for one student
    if (input_student_id)
    {
        i = input_student_id;
        NUM_STUD = i;
    }

    if (DEBUG)
        fprintf(f, "NUM_STUD = %d\n", NUM_STUD);

    //Iterate through records FOR EACH STUDENT, joining and finding courses and CRNs to add to enrollment
    PGresult *enroll_date_res;
    PGresult *maj_res;    
    for (i; i <= NUM_STUD; i++)
    {
        //First, snag the students enrollment date
        char *enroll_date_buffer = (char *) malloc(sizeof(char) * 1024);
        sprintf(enroll_date_buffer, "select s.enrollment_date from registry.student s where s.id=%d;", i);
        enroll_date_res = PQexec(conn, enroll_date_buffer);

        if (PQntuples(enroll_date_res) < 1)
        {
            PQclear(enroll_date_res);
            continue;
        }

        if (PQresultStatus(enroll_date_res) != PGRES_TUPLES_OK)
        {
            free(enroll_date_buffer);
            exit_nicely(conn, "Getting enrollment date for student");
        } 

        char *enroll_date_full = (char*) malloc(sizeof(char) * 15);
        enroll_date_full = PQgetvalue(enroll_date_res, 0, 0);
        int enroll_year, enroll_term;
        enroll_year = get_first_term(enroll_date_full, 0);
        enroll_term = get_first_term(enroll_date_full, 1);

        //Find major(s) for this student (join student & student_major on student_id and student_major & major on major_id
        char *maj_buffer = (char *) malloc(sizeof(char) * 1024);
        sprintf(maj_buffer, "select maj.id from registry.student s join registry.student_major sm on s.id=sm.student_id join registry.major maj on sm.major_id=maj.id where s.id=%d;", i);
        
        maj_res = PQexec(conn, maj_buffer);
        if (PQresultStatus(maj_res) != PGRES_TUPLES_OK)
        {
            free(maj_buffer);
            exit_nicely(conn, "Getting majors for student");
        }

        int NUM_MAJ = PQntuples(maj_res);
        if (DEBUG)
            fprintf(f, "Student %d has %d majors\n", i, NUM_MAJ);

        int majors [10];
        int majoriterate;
        for (majoriterate = 0; majoriterate < NUM_MAJ; majoriterate++)
        {
            majors[majoriterate] = atoi(PQgetvalue(maj_res, majoriterate, 0));
            if (DEBUG)
                fprintf(f, "\tStudents major(s) are: %d\n", majors[majoriterate]);
        }

        int j;
        PGresult *course_of_maj_res;
        for (j = 0; j < NUM_MAJ; j++)
        {
            //access each major as majors[j] 
            char *course_buffer = (char *) malloc(sizeof(char) * 1024);
            sprintf(course_buffer, "select c.id from registry.major m join registry.department d on m.department_id=d.id join registry.course c on d.id=c.department_id where m.id=%d;", majors[j]);
            course_of_maj_res = PQexec(conn, course_buffer);

            if (PQresultStatus(course_of_maj_res) != PGRES_TUPLES_OK)
            {
                free(course_buffer);
                exit_nicely(conn, "Getting course for major");
            } 

            int NUM_COURSES = PQntuples(course_of_maj_res);
            if (DEBUG)
                fprintf(f, "\t\tMajor %d has %d courses\n", majors[j], NUM_COURSES);

            PGresult *insert_res;
            int courses [25];
            int courseiterate;
            for (courseiterate = 0; courseiterate < NUM_COURSES; courseiterate++)
            {
                courses[courseiterate] = atoi(PQgetvalue(course_of_maj_res, courseiterate, 0));
                if (DEBUG)
                    fprintf(f, "\t\t\tMajor %d includes course %d\n", majors[j], courses[courseiterate]);

                //randomly select which courses to continue on this path (to potential registration)
                int threshold = 3; //this number must be beat by rand in order to continue on path
                int rand = rand_lim(30);
                if (rand < threshold)
                    continue;

                //check if course has a prerequisite
                int *prereq_list = (int*)malloc (sizeof(int) * 10);
                int req = check_prereq(conn, courses[courseiterate], prereq_list);
                if (0) //prereq needed
                {
                    //prereq required. See if it's been taken!
                    int cancel = 0;
                    int k = 0;
                    while (prereq_list[k]) 
                    {
                        char *taken_buff = (char*) malloc(sizeof(char) * 1024);
                        sprintf(taken_buff, "select e.student_id from registry.enrollment e join registry.section s on s.crn=e.crn where s.course_id=%d and e.student_id=%d;", prereq_list[k], i);
                        PGresult *taken_res = PQexec(conn, taken_buff);

                        if (PQresultStatus(taken_res) != PGRES_TUPLES_OK)
                        {
                            free(taken_buff);
                            exit_nicely(conn, "Checking which prereqs have been taken");
                        }
 
                        if (PQntuples(taken_res) < 1)
                        {
                            cancel = 1;
                            break;
                        }
                        k++;
                        PQclear(taken_res);
                    }

                    if (cancel > 0)
                        continue;
                }
                else
                { 
                    //check if already enrolled
                    int enrolled = already_enrolled(conn, i, courses[courseiterate]);
                    if (enrolled < 0)
                    {
                        fprintf(f, "%d Has already taken %d! \n", i, courses[courseiterate]);
                        continue;
                    }

                    //Join course and section on course_id, 
                    char *section_buff = (char *) malloc(sizeof(char) * 1024);
                    sprintf(section_buff, "select distinct s.crn, s.quarter, s.year from registry.section s join registry.course c on s.course_id=%d;", courses[courseiterate]);
                    PGresult *section_res = PQexec(conn, section_buff);
 
                    if (PQresultStatus(section_res) != PGRES_TUPLES_OK)
                    {
                        free(section_buff);
                        exit_nicely(conn, "Joining Section & Course");
                    }

                    int NUM_SECTIONS = PQntuples(section_res);
                    if (NUM_SECTIONS < 1)
                    {
                        PQclear(section_res);
                        continue;
                    }

                    //Parse data if all looks good
                    int retry = 3;
                    int enroll_time = 0;
                    int CRN = 0;
                    while((retry >= 0) && (enroll_time == 0) )
                    {
                        retry--;
                        //randomly select a section
                        int rand_sec = rand_lim(NUM_SECTIONS);

                        CRN = atoi(PQgetvalue(section_res, rand_sec, 0));
                        char *quarter = (char*) malloc(sizeof(char) * 10);
                        quarter = PQgetvalue(section_res, rand_sec, 1);
                        int sec_year = atoi(PQgetvalue(section_res, rand_sec, 2));

                        //Section MUST be after enroll_year and enroll_term
                        if (enroll_year > sec_year)
                        {
                            enroll_time = 1;
                            continue;
                        }

                        if (enroll_year == sec_year)
                        {
                            if (strcmp(quarter, "Winter") == 0)
                            {
                                if (enroll_term > 2)
                                {
                                    enroll_time = 1;
                                    continue;
                                }
                            }
                            else if (strcmp(quarter, "Spring") == 0)
                            {
                                if (enroll_term > 4)
                                {
                                    enroll_time = 1;
                                    continue;
                                }
                            }
                            else if (strcmp(quarter, "Summer") == 0)
                            {
                                if (enroll_term > 7)
                                {
                                    enroll_time = 1;
                                    continue;
                                }
                            }
                            else if (strcmp(quarter, "Fall") == 0)
                            {
                                if (enroll_term > 10)
                                {
                                    enroll_time = 1;
                                    continue;
                                }
                            }
                            else
                            {
                                fprintf(stderr, "Error Parsing Term\n");
                            }
                        } 
                        
                        //Finally, check that they have < 4 records for that term
                        char *limit_buff = (char *)malloc(sizeof(char) * 1024);
                        sprintf(limit_buff, "select distinct s.crn from registry.section s join registry.enrollment e on s.crn = e.crn where e.student_id=%d and s.year = %d and s.quarter=\'%s\';", i, sec_year, quarter);
                        PGresult *limit_res = PQexec(conn, limit_buff);

                        if (PQresultStatus(limit_res) != PGRES_TUPLES_OK)
                        {
                            free(limit_buff);
                            exit_nicely(conn, "Checking LIMIT of student classes per term");
                        }

                        int LIM_SECTIONS = PQntuples(limit_res);
                        if (LIM_SECTIONS > 3)
                        {
                            PQclear(limit_res);
                            continue;
                        }

                        //add that student/crn to enrollment
                        char *insert_buff = (char *)malloc (sizeof(char) * 1024);
                        sprintf(insert_buff, "insert into registry.enrollment values(%d, %d);", i, CRN);
                        fprintf(f, "INSERTING: %s\n", insert_buff);
                        insert_res = PQexec(conn, insert_buff);

                        break;
                    }// while retry loop (sections per course)
                }

                //PQclear(insert_res);                

            } //for: check prereq, insert row

            PQclear(course_of_maj_res);

        } //for: courses per major

        PQclear(enroll_date_res);
        PQclear(maj_res);
    } //for: majors per student
 
    free(student_buffer);
    PQclear(res);
    PQfinish(conn);
    return 0;
}

int get_first_term(char *enroll_date, int year_or_term)
{
    //parse enroll_date and return
    char *buffer = (char *) malloc(sizeof(char) * 20);
    int start, finish;

    if (year_or_term == 0) //parse for year
    {
        start = 0;
        finish = 3;
    }
    else //parse for month
    {
        start = 5;
        finish = 6;
    }
    int i = 0;
    for (start; start <= finish; start++)
    {
        buffer[i] = enroll_date[start];
        i++;
    }

    return atoi(buffer);
}


int check_prereq(PGconn *conn, int course, int *prereqlist)
{
    char *prereq_buffer = (char *) malloc (sizeof(char) * 1024);
    sprintf(prereq_buffer, "select p.course_id from registry.course c join registry.prerequisite p on c.id=p.course_id where c.id=%d", course);
    PGresult *prereq_res = PQexec(conn, prereq_buffer);

    if (PQresultStatus(prereq_res) != PGRES_TUPLES_OK)
    {
        free(prereq_buffer);
        exit_nicely(conn, "Checking course prereqs");
    }

    int NUM_REQ = PQntuples(prereq_res);
    if (NUM_REQ > 0)
    {
        int i;
        for (i = 0; i < NUM_REQ; i++)
        {
            prereqlist[i] = atoi(PQgetvalue(prereq_res, i, 0));
        }
        PQclear(prereq_res);
        return -1;
    }

    PQclear(prereq_res);
    return 0;
}

int already_enrolled(PGconn *conn, int student, int course)
{
    char *buffer = (char *) malloc (sizeof(char) * 1024);
    sprintf(buffer, "select e.crn from registry.enrollment e join registry.section s on e.crn=s.crn join registry.course c on s.course_id=c.id where e.student_id =%d and c.id=%d;", student, course);
    PGresult *res = PQexec(conn, buffer);

    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        free(buffer);
        exit_nicely(conn, "Checking if already enrolled");
    }

    if (PQntuples(res) < 1)
    {
        PQclear(res);
        return 0;
    }

    PQclear(res);
    return -1;
}

int rand_lim(int limit) 
{
    //return a random number between 0 and limit inclusive

    int divisor = RAND_MAX/(limit+1);
    int retval;

    do { 
        retval = rand() / divisor;
    } while (retval > limit);

    if (retval > 0)
        return retval -1;
    else
        return retval;
}
