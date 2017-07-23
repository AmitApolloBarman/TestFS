//
//  main.c
//  TestFS
//
//  Created by Amit Apollo Barman on 7/22/17.
//  Copyright © 2017 Amit Apollo Barman. All rights reserved.
//

#include <stdio.h>
#include <CoreServices/CoreServices.h>
#include "file_fswatch.h"

int main(int argc, char *argv[]) {
    return do_eventfs(argc, argv);
}
