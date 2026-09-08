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
extern purrgo_gnss_solution_t gnss_solution;
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
  * @brief  Gets Time from RTC
  * @param  None
  * @retval Time in DWORD
  */
DWORD get_fattime(void)
{
  /* USER CODE BEGIN get_fattime */
  // Use a fallback date of 1 Jan 2026 00:00:00 if GNSS is invalid or year is 0
  // This ensures a valid FAT timestamp is generated even without a satellite fix.
  if (!gnss_solution.valid || gnss_solution.year == 0) {
      uint32_t year = 2026;
      uint32_t month = 1;
      uint32_t day = 1;
      uint32_t hour = 0;
      uint32_t minute = 0;
      uint32_t second = 0;

      return ((DWORD)(year - 1980) << 25)
           | ((DWORD)month << 21)
           | ((DWORD)day << 16)
           | ((DWORD)hour << 11)
           | ((DWORD)minute << 5)
           | ((DWORD)(second / 2));
  }

  // GNSS year is a two-digit year (e.g. 26 for 2026). Convert to full year
  // using a 32-bit integer to prevent 8-bit overflow during addition.
  uint32_t year = 2000 + gnss_solution.year;
  uint32_t month = gnss_solution.month;
  uint32_t day = gnss_solution.day;
  uint32_t hour = gnss_solution.hours;
  uint32_t minute = gnss_solution.minutes;
  uint32_t second = gnss_solution.seconds;

  // FAT stores seconds with 2-second resolution, so we must divide the
  // GNSS seconds by 2 (truncating odd seconds).
  return ((DWORD)(year - 1980) << 25)
       | ((DWORD)month << 21)
       | ((DWORD)day << 16)
       | ((DWORD)hour << 11)
       | ((DWORD)minute << 5)
       | ((DWORD)(second / 2));
  /* USER CODE END get_fattime */
}

/* USER CODE BEGIN Application */

/* USER CODE END Application */
