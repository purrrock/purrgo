/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file   fatfs.c
  * @brief  Code for fatfs applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
#include "fatfs.h"

uint8_t retUSER;    /* Return value for USER */
char USERPath[4];   /* USER logical drive path */
FATFS USERFatFS;    /* File system object for USER logical drive */
FIL USERFile;       /* File object for USER */

/* USER CODE BEGIN Variables */
#include "purrgo/gnss_types.h"


/* USER CODE END Variables */

void MX_FATFS_Init(void)
{
  /*## FatFS: Link the USER driver ###########################*/
  retUSER = FATFS_LinkDriver(&USER_Driver, USERPath);

  /* USER CODE BEGIN Init */
  /* additional user code for init */
  /* USER CODE END Init */
}

/**
  * @brief  Gets Time from RTC (Uses GNSS time for FAT timestamps)
  * @param  None
  * @retval Time in DWORD
  */
DWORD get_fattime(void)
{
  /* USER CODE BEGIN get_fattime */
  extern purrgo_gnss_solution_t gnss_solution;

  // If there is no valid GNSS solution yet or the year is 0,
  // return a deterministic default timestamp: January 1, 2026, 00:00:00.
  if (!gnss_solution.valid || gnss_solution.year == 0) {
      return ((DWORD)(2026 - 1980) << 25)
           | ((DWORD)1 << 21)
           | ((DWORD)1 << 16)
           | ((DWORD)0 << 11)
           | ((DWORD)0 << 5)
           | ((DWORD)(0 / 2));
  }

  uint32_t year = gnss_solution.year;
  // If the GNSS solution contains a two-digit year, convert it to a full year.
  if (year < 100) {
      year += 2000;
  }

  // Pack the GNSS date/time into the FAT timestamp bit layout:
  // bits 31..25: year offset from 1980
  // bits 24..21: month
  // bits 20..16: day
  // bits 15..11: hour
  // bits 10..5: minute
  // bits 4..0: seconds divided by 2
  return ((DWORD)(year - 1980) << 25)
       | ((DWORD)gnss_solution.month << 21)
       | ((DWORD)gnss_solution.day << 16)
       | ((DWORD)gnss_solution.hours << 11)
       | ((DWORD)gnss_solution.minutes << 5)
       | ((DWORD)(gnss_solution.seconds / 2));
  /* USER CODE END get_fattime */
}


/* USER CODE BEGIN Application */

/* USER CODE END Application */
