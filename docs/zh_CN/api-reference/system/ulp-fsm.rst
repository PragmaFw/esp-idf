ULP FSM 协处理器编程
====================

:link_to_translation:`en:[English]`

ULP（Ultra Low Power，超低功耗）协处理器是一种简单的有限状态机 (FSM)，可以在主处理器处于深度睡眠模式时，使用 ADC、温度传感器和外部 I2C 传感器执行测量操作。ULP 协处理器可以访问 ``RTC_SLOW_MEM`` 内存区域及 ``RTC_CNTL``、``RTC_IO``、``SARADC`` 外设中的寄存器。ULP 协处理器使用 32 位固定宽度的指令，32 位内存寻址，配备 4 个 16 位通用寄存器。在 ESP-IDF 中，此协处理器被称作 ``ULP FSM``。

.. only:: esp32s2 or esp32s3

    {IDF_TARGET_NAME} 基于 RISC-V 指令集架构提供另一种 ULP 协处理器。关于 ``ULP RISC-V`` 的详细信息，请参考 :doc:`ULP-RISC-V Coprocessor <../../../api-reference/system/ulp-risc-v>`。

安装工具链
----------

ULP FSM 协处理器代码由汇编语言编写，使用 `binutils-esp32ulp 工具链`_ 进行编译。

按照 :doc:`快速入门指南 <../../../get-started/index>` 的步骤配置 ESP-IDF 及其 CMake 构建系统时，默认安装 ULP FSM 工具链。

编写 ULP FSM
-------------------

使用受支持的指令集即可编写 ULP FSM 协处理器，此外也可使用主处理器上的 C 语言宏进行编程。以下小节分别介绍了这两种方法：

.. toctree::
   :maxdepth: 1

   {IDF_TARGET_NAME} ULP 指令集参考 <ulp_instruction_set>
   使用宏进行编程（旧版方法） <ulp_macros>

编译 ULP 代码
--------------

要将 ULP FSM 代码作为某组件的一部分进行编译，须执行以下步骤：

1. 用汇编语言编写的 ULP FSM 代码必须写入到一个或多个 ``.S`` 扩展文件中，且这些文件必须放在组件目录下的一个独立子目录中，例如 ``ulp/``。

.. note::

    在注册组件（通过 ``idf_component_register``）时，不应将该目录添加到 ``SRC_DIRS`` 参数中，因为 ESP-IDF 构建系统会根据文件扩展名来编译 ``SRC_DIRS`` 中包含的文件。对于 ``.S`` 文件，将使用 ``{IDF_TARGET_TOOLCHAIN_PREFIX}-as`` 汇编器进行编译，但这并不适用于 ULP FSM 汇编文件。因此，区分这类文件最简单的方式就是将 ULP FSM 汇编文件放到单独的目录中。同样，ULP FSM 汇编源文件也 **不应该** 添加到 ``SRCS`` 中。请参考如下步骤，查看如何正确添加 ULP FSM 汇编源文件。

2. 注册后从组件 CMakeLists.txt 中调用 ``ulp_embed_binary``。示例如下::

    ...
    idf_component_register()

    set(ulp_app_name ulp_${COMPONENT_NAME})
    set(ulp_s_sources ulp/ulp_assembly_source_file.S)
    set(ulp_exp_dep_srcs "ulp_c_source_file.c")

    ulp_embed_binary(${ulp_app_name} "${ulp_s_sources}" "${ulp_exp_dep_srcs}")

``ulp_embed_binary`` 的第一个参数用于指定 ULP FSM 二进制文件的名称，该名称也用于生成的其他文件，如：ELF 文件、.map 文件、头文件和链接器导出文件。第二个参数指定 ULP FSM 汇编源文件。最后，第三个参数指定组件源文件列表，这些源文件中会包含要生成的头文件。此列表用于建立正确的依赖项，并确保在编译这些文件之前先创建生成的头文件。有关 ULP FSM 应用程序生成的头文件等相关概念，请参考下文。

在这个生成的头文件中，ULP 代码中的变量默认以 ``ulp_`` 作为前缀。

如果需要嵌入多个 ULP 程序，可以添加自定义前缀，以避免变量名冲突，如下所示：

.. code-block:: cmake

    idf_component_register()

    set(ulp_app_name ulp_${COMPONENT_NAME})
    set(ulp_sources "ulp/ulp_c_source_file.c" "ulp/ulp_assembly_source_file.S")
    set(ulp_exp_dep_srcs "ulp_c_source_file.c")

    ulp_embed_binary(${ulp_app_name} "${ulp_sources}" "${ulp_exp_dep_srcs}" PREFIX "ULP::")

最后的 PREFIX 参数可以是 C 语言风格命名的前缀（如 ``ulp2_``）或 C++ 风格命名的前缀（如 ``ULP::``）。

3. 继续使用常规方法（例如 ``idf.py app``）编译应用程序。

    在内部，构建系统将按照以下步骤编译 ULP FSM 程序：

    1. **通过 C 预处理器运行每个程序集文件 (foo.S)。** 此步骤在组件编译目录中生成预处理的汇编文件 (foo.ulp.S)，同时生成依赖文件 (foo.ulp.d)。

    2. **通过汇编器运行预处理过的汇编源码。** 此步骤会生成目标文件 (foo.ulp.o) 和清单 (foo.ulp.lst)。清单文件仅用于调试，不用于编译进程的后续步骤。

    3. **通过 C 预处理器运行链接器脚本模板。** 模板位于 ``components/ulp/ld`` 目录中。

    4. **将目标文件链接到 ELF 输出文件** (``ulp_app_name.elf``)。此步骤生成的 .map 文件 (``ulp_app_name.map``) 默认用于调试。

    5. **将 ELF 文件中的内容转储为二进制文件** (``ulp_app_name.bin``)，以便嵌入到应用程序中。

    6. 使用 ``esp32ulp-elf-nm`` 从 ELF 文件中 **生成全局符号列表** (``ulp_app_name.sym``)。

    7. **创建 LD 导出脚本和头文件** （``ulp_app_name.ld`` 和 ``ulp_app_name.h``），包含来自 ``ulp_app_name.sym`` 的符号。此步骤使用 ``esp32ulp_mapgen.py`` 工具来完成。

    8. **将生成的二进制文件添加到要嵌入应用程序的二进制文件列表中。**

访问 ULP FSM 程序变量
------------------------

在 ULP FSM 程序中定义的全局符号也可以在主程序中使用。

例如，ULP FSM 程序可以定义 ``measurement_count`` 变量，此变量可以定义程序从深度睡眠中唤醒芯片之前需要进行的 ADC 测量的次数::

                            .global measurement_count
    measurement_count:      .long 0

                            // later, use measurement_count
                            move r3, measurement_count
                            ld r3, r3, 0

主程序需要在启动 ULP 程序之前初始化该 ``measurement_count`` 变量，构建系统通过生成定义 ULP 编程中全局符号的 ``${ULP_APP_NAME}.h`` 和 ``${ULP_APP_NAME}.ld`` 文件实现上述操作。在 ULP 程序中定义的所有全局符号都包含在这两个文件中，且都以 ``ulp_`` 开头。

头文件包含对此类符号的声明::

    extern uint32_t ulp_measurement_count;

注意，所有符号（包括变量、数组、函数）均被声明为 ``uint32_t``。对于函数和数组，先获取符号地址，然后转换为适当的类型。

生成的链接器脚本文件定义了 RTC_SLOW_MEM 中的符号位置::

    PROVIDE ( ulp_measurement_count = 0x50000060 );

如果要从主程序访问 ULP 程序变量，应先使用 ``include`` 语句包含生成的头文件，这样，就可以像访问常规变量一样访问 ulp 程序变量。操作如下::

    #include "ulp_app_name.h"

    // later
    void init_ulp_vars() {
        ulp_measurement_count = 64;
    }

.. only:: esp32

    注意，ULP FSM 程序在 RTC 内存中只能使用 32 位字的低 16 位，因为寄存器是 16 位的，并且不具备从字的高位加载的指令。同样，ULP 储存指令将寄存器值写入 RTC 内存中 32 位字的低 16 位。高 16 位写入的值取决于储存指令的地址，因此在读取 ULP 协处理器写的变量时，主应用程序需要屏蔽高 16 位，例如：

::

    printf("Last measurement value: %d\n", ulp_last_measurement & UINT16_MAX);

启动 ULP FSM 程序
--------------------

要运行 ULP FSM 程序，主应用程序需要调用 :cpp:func:`ulp_load_binary` 函数将 ULP 程序加载到 RTC 内存中，然后调用 :cpp:func:`ulp_run` 函数，启动 ULP 程序。

注意，在 menuconfig 中必须启用 ``Enable Ultra Low Power (ULP) Coprocessor`` 选项，以便正常运行 ULP，并且必须设置 ``ULP Co-processor type`` 选项，以便选择要使用的 ULP 类型。 ``RTC slow memory reserved for coprocessor`` 选项设置的值必须足够储存 ULP 代码和数据。如果应用程序组件包含多个 ULP 程序，则 RTC 内存必须足以容纳最大的程序。

每个 ULP 程序均以二进制 BLOB 的形式嵌入到 ESP-IDF 应用程序中。应用程序可以引用此 BLOB，并以下面的方式加载此 BLOB（假设 ULP_APP_NAME 已被定义为 ``ulp_app_name``）::

    extern const uint8_t bin_start[] asm("_binary_ulp_app_name_bin_start");
    extern const uint8_t bin_end[]   asm("_binary_ulp_app_name_bin_end");

    void start_ulp_program() {
        ESP_ERROR_CHECK( ulp_load_binary(
            0 // load address, set to 0 when using default linker scripts
            bin_start,
            (bin_end - bin_start) / sizeof(uint32_t)) );
    }

一旦上述程序加载到 RTC 内存后，应用程序即可将入口点的地址传递给 ``ulp_run`` 函数，以启动此程序::

    ESP_ERROR_CHECK( ulp_run(&ulp_entry - RTC_SLOW_MEM) );

入口点符号的声明来自上述生成的头文件 ``${ULP_APP_NAME}.h``。在 ULP FSM 应用程序的汇编源代码中，此符号必须标记为 ``.global``::


            .global entry
    entry:
            // code starts here

.. only:: esp32

    ESP32 ULP 程序流
    -------------------

    ESP32 ULP 协处理器由定时器启动，而调用 :cpp:func:`ulp_run` 则可启动此定时器。定时器为 RTC_SLOW_CLK 的 Tick 事件计数（默认情况下，Tick 由内部 150 kHz RC 振荡器生成）。使用 ``SENS_ULP_CP_SLEEP_CYCx_REG`` 寄存器 (x = 0..4) 设置 Tick 数值。第一次启动 ULP 时，使用 ``SENS_ULP_CP_SLEEP_CYC0_REG`` 设置定时器 Tick 数值，之后，ULP 程序可以使用 ``sleep`` 指令来选择另一个 ``SENS_ULP_CP_SLEEP_CYCx_REG`` 寄存器。

    此应用程序可以调用 ``ulp_set_wakeup_period`` 函数来设置 ULP 定时器周期值 (SENS_ULP_CP_SLEEP_CYCx_REG, x = 0..4)。

    一旦定时器计数到 ``SENS_ULP_CP_SLEEP_CYCx_REG`` 寄存器设定的 Tick 数值，ULP 协处理器就会启动，并调用 :cpp:func:`ulp_run` 的入口点开始运行程序。

    程序保持运行，直到遇到 ``halt`` 指令或非法指令。一旦程序停止，ULP 协处理器电源关闭，定时器再次启动。

    如果想禁用定时器（有效防止 ULP 程序再次运行），可在 ULP 代码或主程序中清除 ``RTC_CNTL_STATE0_REG`` 寄存器中的 ``RTC_CNTL_ULP_CP_SLP_TIMER_EN`` 位。


.. only:: esp32s2 or esp32s3

    {IDF_TARGET_NAME} ULP 程序流
    ----------------------------

    {IDF_TARGET_NAME} ULP 协处理器由定时器启动，调用 :cpp:func:`ulp_run` 则可启动此定时器。定时器为 RTC_SLOW_CLK 的 Tick 事件计数（默认情况下，Tick 由内部 90 KHz RC 振荡器生成）。使用 ``RTC_CNTL_ULP_CP_TIMER_1_REG`` 寄存器设置 Tick 数值。

    此应用程序可以调用 :cpp:func:`ulp_set_wakeup_period` 函数来设置 ULP 定时器周期值。

    一旦定时器计数到 ``RTC_CNTL_ULP_CP_TIMER_1_REG`` 寄存器设定的 Tick 数值，ULP 协处理器就会启动，并调用 :cpp:func:`ulp_run` 的入口点开始运行程序。

    程序保持运行，直到遇到 ``halt`` 指令或非法指令。一旦程序停止，ULP 协处理器电源关闭，定时器再次启动。

    如果想禁用定时器（有效防止 ULP 程序再次运行），可在 ULP 代码或主程序中清除 ``RTC_CNTL_ULP_CP_TIMER_REG`` 寄存器中的 ``RTC_CNTL_ULP_CP_SLP_TIMER_EN`` 位。

应用示例
--------------------

* :example:`system/ulp/ulp_fsm/ulp` 展示了主处理器运行其他代码或处于 Deep-sleep 状态时，使用 ULP FSM 协处理器对 IO 脉冲进行计数，脉冲计数会在唤醒时保存到 NVS 中。

.. only:: esp32 or esp32s3

    * :example:`system/ulp/ulp_fsm/ulp_adc` 展示了主处理器处于 Deep-sleep 状态时，ULP FSM 协处理器测量特定 ADC 通道上的输入电压，将其与设定的阈值进行比较，电压超出阈值时唤醒系统。

API 参考
-------------

.. include-build-file:: inc/ulp_fsm_common.inc
.. include-build-file:: inc/ulp_common.inc

.. _binutils-esp32ulp 工具链: https://github.com/espressif/binutils-gdb
