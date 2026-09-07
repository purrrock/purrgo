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
#include "purrgo/gnss_adapter.h"
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
  /*
   * Use the GNSS time to ensure SD card log files and other files
   * get the correct creation and modification timestamps.
   *
   * FAT timestamp bit layout:
   * bits 31..25: Year from 1980 (0..127 = 1980..2107)
   * bits 24..21: Month (1..12)
   * bits 20..16: Day (1..31)
   * bits 15..11: Hour (0..23)
   * bits 10..5:  Minute (0..59)
   * bits 4..0:   Second divided by 2 (0..29)
   */
  const purrgo_gnss_solution_t* sol = purrgo_gnss_get_solution();

  /*
   * If there is no valid GNSS fix or the date hasn't been set,
   * return a deterministic default timestamp: 2026-01-01 00:00:00.
   */
  if (!sol || !sol->valid || sol->year == 0) {
      return ((DWORD)(2026 - 1980) << 25)
           | ((DWORD)1 << 21)
           | ((DWORD)1 << 16)
           | ((DWORD)0 << 11)
           | ((DWORD)0 << 5)
           | ((DWORD)0);
  }

  uint16_t year = sol->year;

  /*
   * The GNSS solution year is an offset from 2000 (e.g. '26' -> 2026).
   */
  if (year < 100) {
      year += 2000;
  }

  return ((DWORD)(year - 1980) << 25)
       | ((DWORD)sol->month << 21)
       | ((DWORD)sol->day << 16)
       | ((DWORD)sol->hours << 11)
       | ((DWORD)sol->minutes << 5)
       | ((DWORD)(sol->seconds / 2));
  /* USER CODE END get_fattime */
}

/* USER CODE BEGIN Application */

/* USER CODE END Application */
