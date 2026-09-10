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
#include "purrgo/app_fsm.h"
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
  * @brief  Gets Time from RTC
  * @param  None
  * @retval Time in DWORD
  */
DWORD get_fattime(void)
{
  /* USER CODE BEGIN get_fattime */
  const purrgo_gnss_solution_t* gnss = purrgo_app_get_gnss_solution();
  uint32_t year, month, day, hour, minute, second;

  if (gnss != NULL && gnss->valid && gnss->year > 0)
  {
    year = 2000 + gnss->year;
    month = gnss->month;
    day = gnss->day;
    hour = gnss->hours;
    minute = gnss->minutes;
    second = gnss->seconds;
  }
  else
  {
    // Use a fallback date of 1 Jan 2026 00:00:00
    // This ensures a valid FAT timestamp is generated even without a satellite fix.
    year = 2026;
    month = 1;
    day = 1;
    hour = 0;
    minute = 0;
    second = 0;
  }

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
