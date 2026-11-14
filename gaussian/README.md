# Gaussian Blur Sequential và OpenMP

Trong terminal Ubuntu, chạy `bash test_one.sh 1024 image_1.bin` để build, đo và tạo ảnh trước/sau. Chạy `bash run_full.sh` để đo cả bộ dữ liệu và xuất `summary.csv`. [Hướng dẫn từ thư mục dự án](../README.md)

Người 1 phụ trách Gaussian Blur. Tài liệu nhóm quy định hai tệp nộp `gaussian_seq.cpp` và `gaussian_omp.cpp`, ba kích thước ảnh, bốn cấu hình OpenMP, một lượt warm-up và mười lượt đo cho mỗi cấu hình. Bảng kết quả gồm Execution Time, Speedup, Parallel Efficiency và Correctness. [Tài liệu phân công](../refer/T%C3%ADnh%20to%C3%A1n%20song%20song.docx)

Lượt đo bàn giao dùng kernel 19 × 19, σ = 3 và nằm trong [`results/full_k19_s3`](results/full_k19_s3). Có thể đưa mã nguồn và script từ `cki/gaussian` lên GitHub cùng cấu trúc dữ liệu của nhóm. `.gitignore` loại các tệp biên dịch và thư mục kết quả phát sinh khỏi lần thêm mã thông thường.

## Dữ liệu dùng chung

| Đường dẫn tương đối từ `cki` | Dữ liệu đã kiểm tra |
| --- | --- |
| `data/raw` | 20 JPEG RGB. Chiều rộng 2069–4160 và chiều cao 2379–6338 pixel sau khi áp dụng hướng EXIF. |
| `data/processed_raw_names/512`, `1024`, `2048` | 20 tệp `.bin` mỗi kích thước, giữ tên gốc. Mỗi tệp lưu ảnh grayscale theo hàng, một số float32 little-endian mỗi pixel, miền [0,255], không có header. |
| `data/processed_raw_names/preview` | 20 PNG grayscale 512 × 512, giữ tên gốc. |
| `data/processed_raw_names/metadata.csv` | Tên ảnh gốc, kích thước, tọa độ crop và định dạng. `image_2.jpg` tạo `image_2.bin`, rồi benchmark và script xuất PNG tạo `image_2.png`. |

Chương trình đọc trực tiếp dữ liệu đã chuẩn bị. Để chuẩn bị dữ liệu giữ tên RAW, chạy script với Pillow và NumPy từ thư mục `cki/gaussian`.

```powershell
python -m pip install pillow numpy
python ../scripts/preprocess.py --input ../data/raw --output ../data/processed_raw_names --preview
```

## Mã nguồn và phương pháp

| Tệp | Vai trò |
| --- | --- |
| `gaussian.cpp`, `gaussian.hpp` | Tạo kernel, khai báo API và thực hiện tích chập một hàng. |
| `gaussian_seq.cpp`, `gaussian_omp.cpp` | Duyệt tuần tự hoặc phân hàng cho OpenMP. Cả hai gọi cùng hàm tích chập. |
| `image_io.cpp`, `filter_main.cpp` | Kiểm tra và đọc `.bin`, chạy một ảnh, lưu ảnh grayscale 8-bit dạng PGM. |
| `benchmark.cpp`, `plot_results.py` | Đo tự động, đối chiếu pixel, xuất CSV và vẽ ba biểu đồ. |
| `export_png.py` | Chuyển toàn bộ ảnh đầu ra PGM sang PNG grayscale bằng Pillow. |
| `test_gaussian.py` | Kiểm thử đầu ra chính xác cho ảnh nhỏ, biên, kernel lớn hơn ảnh và các đầu vào không hợp lệ. |

Kernel mặc định là 19 × 19 với σ = 3, tương ứng bán kính 9 pixel. Với mỗi độ lệch `(dx, dy)`, mã lấy mẫu `exp(-(dx² + dy²)/(2σ²))` và chia từng trọng số cho tổng. Hệ số `1/(2πσ²)` triệt tiêu khi chuẩn hóa. Kernel được tạo một lần cho mỗi cấu hình. Do kernel đối xứng khi xoay 180°, phép lấy mẫu với độ lệch dương cho kết quả tích chập tương đương. Replicate padding giới hạn từng tọa độ vào biên ảnh. Tổng có trọng số được tính bằng `double`, giới hạn về [0,255], rồi làm tròn tới số nguyên gần nhất với giá trị nửa làm tròn lên. [Gonzalez và Woods, *Digital Image Processing*, ấn bản 4, §3.4–3.5](../refer/Gonzales,Woods-Digital.Image.Processing.4th.Edition.pdf)

Tích chập trực tiếp thực hiện `k²` phép nhân cộng cho mỗi pixel, với độ phức tạp `O(WHk²)`. OpenMP dùng `schedule(static)` để chia các hàng. Các luồng đọc chung ảnh đầu vào và kernel. Mỗi vòng lặp ghi một hàng riêng và biến tổng thuộc về pixel đang tính. Chương trình tắt điều chỉnh số luồng động và kiểm tra số luồng thực tế cho mỗi lượt. Cách phân dải hàng và phạm vi đo tham khảo §§2, 3 và 6 của bài báo Yu và cộng sự. Bài báo khảo sát truyền thông giữa các tiến trình. Bản OpenMP dùng bộ nhớ chia sẻ để đọc lân cận qua ranh giới dải hàng. [*Parallel Algorithms for Image Convolution*](../refer/Parallel_Algorithms_For_Image_Convolution.pdf)

## Biên dịch trên Windows bằng GCC

Cần GCC x64 hỗ trợ C++17 và OpenMP. C++ đọc định dạng của nhóm bằng thư viện chuẩn. Python chỉ cần khi chạy bộ kiểm thử hoặc xuất biểu đồ.

Nếu `Get-Command g++` không tìm thấy compiler, cài MSYS2 và mở terminal **MSYS2 UCRT64**. Chạy `pacman -Syu`. Nếu terminal yêu cầu đóng để cập nhật, mở lại UCRT64 và chạy `pacman -Syu` lần nữa. Cài GCC bằng `pacman -S --needed mingw-w64-ucrt-x86_64-gcc`. [Hướng dẫn MSYS2 của MinGW-w64](https://www.mingw-w64.org/getting-started/msys2/)

Trong PowerShell, với MSYS2 cài ở đường dẫn mặc định, dùng các lệnh sau. Nếu đã có GCC x64 trên `PATH`, có thể bỏ dòng sửa `PATH`.

```powershell
Set-Location D:\ttss\cki\gaussian
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
g++ --version
.\build_gcc.ps1
```

Hai lệnh biên dịch trực tiếp cho chương trình chạy một ảnh. Script ở trên còn xây dựng `gaussian_bench.exe` và dùng cùng cờ tối ưu hóa cho cả hai bản.

```powershell
New-Item -ItemType Directory -Force build | Out-Null
g++ -std=c++17 -O3 -DNDEBUG -ffp-contract=off filter_main.cpp image_io.cpp gaussian.cpp gaussian_seq.cpp -o build/gaussian_seq.exe
g++ -std=c++17 -O3 -DNDEBUG -ffp-contract=off -fopenmp -DGAUSSIAN_USE_OPENMP filter_main.cpp image_io.cpp gaussian.cpp gaussian_omp.cpp openmp_runtime.cpp -o build/gaussian_omp.exe
```

## Chạy một ảnh và kiểm tra

```powershell
$data = '..\data\processed_raw_names'
$inputImage = Join-Path $data '512\image_2.bin'
.\build\gaussian_seq.exe $inputImage 512 512 .\build\trial_seq.pgm
.\build\gaussian_omp.exe $inputImage 512 512 .\build\trial_omp.pgm 19 3 8
Get-FileHash .\build\trial_seq.pgm, .\build\trial_omp.pgm
python .\test_gaussian.py .\build\gaussian_seq.exe .\build\gaussian_omp.exe
```

Hai ảnh PGM có cùng SHA-256 khi đầu ra giống nhau từng byte. Dùng Pillow để đổi PGM sang PNG nếu trình xem ảnh của bạn cần PNG.

```powershell
python -m pip install pillow
python -c "from PIL import Image; Image.open('build/trial_seq.pgm').save('build/trial_seq.png')"
```

Tham số theo thứ tự là ảnh `.bin`, chiều rộng, chiều cao, tệp PGM đích, kích thước kernel lẻ dương, sigma dương và số luồng với bản OpenMP. Có thể bỏ các tham số cuối để dùng kernel 19 × 19, sigma 3 và 4 luồng. Mỗi lần chạy cần tên PGM mới.

## Benchmark và biểu đồ

```powershell
New-Item -ItemType Directory -Force results | Out-Null
$data = '..\data\processed_raw_names'
.\build\gaussian_bench.exe $data .\results\single_windows 19 3 image_2.bin
.\build\gaussian_bench.exe $data .\results\full_windows
python -m pip install matplotlib
python .\plot_results.py .\results\full_windows
```

Tham số `image_2.bin` đo một ảnh ở cả ba kích thước. Bỏ tham số này để đo đủ 20 ảnh mỗi kích thước. Có thể thay `$data` bằng đường dẫn tuyệt đối đến thư mục chứa ba thư mục kích thước. Mỗi lượt chạy cần tên thư mục kết quả mới. Trên máy benchmark chung, lưu thêm thông tin hệ điều hành, CPU và lệnh biên dịch đã dùng.

```powershell
$out = '.\results\full_windows'
@(
    (Get-Date -Format o)
    (Get-CimInstance Win32_OperatingSystem | Select-Object Caption, Version, BuildNumber | Out-String)
    (Get-CimInstance Win32_Processor | Select-Object Name, NumberOfCores, NumberOfLogicalProcessors | Out-String)
    (g++ --version | Out-String)
    (Get-Content .\build_gcc.ps1 -Raw)
) | Set-Content -Encoding utf8 (Join-Path $out 'environment_windows.txt')
```

Mỗi ảnh và kích thước chạy Sequential và OpenMP với 1, 2, 4 và 8 luồng. Mỗi cấu hình warm-up một lần, rồi đo 10 lần bằng `std::chrono::steady_clock`. Cấu hình bắt đầu thay đổi sau mỗi ảnh. Phạm vi đo là lời gọi bộ lọc, gồm chi phí vào và ra vùng OpenMP và làm tròn pixel. Đọc dữ liệu, dựng kernel, cấp phát bộ đệm, ghi tệp, so sánh ảnh và vẽ biểu đồ đều ở ngoài khoảng đo. Hai bản cùng tối ưu hóa `-O3`, dùng chung mã tích chập, tắt FP contraction và xây dựng đối tượng Sequential của benchmark không có cờ OpenMP.

| Tệp trong thư mục kết quả | Nội dung |
| --- | --- |
| `timings_raw.csv` | 3.000 lần đo chính thức khi chạy toàn bộ. |
| `metrics.csv` | 300 cấu hình với median ms. Dòng OpenMP có speedup `Tseq/Tomp`, efficiency `100 × speedup/P`, MaxAE và RMSE so với đầu ra Sequential của chính ảnh đó. |
| `mismatches.csv` | Số pixel lệch và tọa độ, giá trị của pixel lệch đầu tiên nếu có. Benchmark trả mã 2 khi phát hiện sai khác. |
| `run_config.txt` | Dataset, kernel, sigma, kích thước, số luồng, OpenMP và biến môi trường ảnh hưởng đến runtime. |
| `samples/` | Một ảnh đầu vào, ảnh Sequential và ảnh OpenMP 8 luồng ở mỗi kích thước. |
| `images/<size>/<mode>/` | Tất cả ảnh đã lọc. Mỗi chế độ `seq`, `omp1`, `omp2`, `omp4`, `omp8` lưu một tệp PGM cho mỗi ảnh. |
| `execution_time.png`, `speedup.png`, `parallel_efficiency.png` | Ba biểu đồ. Mỗi điểm là median qua các ảnh cùng kích thước của metric tương ứng trong CSV. Đường ngang nét đứt trên biểu đồ thời gian là baseline Sequential của từng kích thước. |

### Xuất toàn bộ ảnh sang PNG

Biên dịch lại trước khi chạy benchmark nếu đang dùng bản cũ. Lượt benchmark đầy đủ lưu **300 ảnh PGM** trong `images/` (60 đầu vào × 5 chế độ), chiếm khoảng 525 MiB. Chạy thêm lệnh sau trong PowerShell tại thư mục `cki/gaussian` để lưu thêm 300 PNG cùng tên bên cạnh các tệp PGM. Thay đường dẫn bằng thư mục kết quả của lượt cần xuất.

```powershell
python -m pip install pillow
python .\export_png.py .\results\full_windows
```

Ví dụ, `images/2048/omp8/image_2.png` là kết quả OpenMP 8 luồng ở 2048 × 2048 của `image_2.jpg`. PNG lưu cùng giá trị pixel với PGM. Benchmark ghi ảnh sau khi đo xong từng ảnh, còn chuyển PNG thực hiện sau benchmark.

## Chạy bằng Ubuntu WSL2

Từ PowerShell, chọn bản phân phối Ubuntu đã có sẵn. Trong Ubuntu, nếu thiếu công cụ, cài bằng `sudo apt update` và `sudo apt install build-essential python3`.

```powershell
wsl.exe --list --verbose
wsl.exe -d Ubuntu --cd /mnt/d/ttss/cki/gaussian
```

```bash
bash build_gcc.sh
python3 test_gaussian.py build/gaussian_seq build/gaussian_omp
mkdir -p results
./build/gaussian_bench ../data/processed_raw_names results/full_wsl2_rerun
{ date -Iseconds; uname -srmo; cat /etc/os-release; lscpu; g++ --version; ldd build/gaussian_bench; cat build_gcc.sh; } > results/full_wsl2_rerun/environment_wsl2.txt
```

Để xuất PNG ngay trong terminal Ubuntu, cài `sudo apt install python3-pil` nếu thiếu Pillow, rồi chạy:

```bash
python3 export_png.py results/full_wsl2_rerun
```

Cũng có thể xuất PNG và vẽ biểu đồ từ PowerShell bằng `python .\export_png.py .\results\full_wsl2_rerun` và `python .\plot_results.py .\results\full_wsl2_rerun`. Lưu kết quả WSL2 và Windows native theo từng thư mục chạy để giữ thông tin môi trường đi kèm.

## Lỗi thường gặp

- **`g++` hoặc `omp.h` không tìm thấy.** Kiểm tra `Get-Command g++` và cài gói GCC trong UCRT64 nếu thiếu. Dùng `-fopenmp` khi biên dịch và liên kết mã OpenMP.
- **Thiếu DLL OpenMP trên Windows.** Giữ thư mục `ucrt64\bin` của đúng bộ GCC trong `PATH` của PowerShell đang chạy.
- **Application Control chặn chạy `.exe`.** Dùng Ubuntu WSL2 sẵn có hoặc môi trường thực thi được máy nhóm cho phép. Lượt chạy bàn giao dùng WSL2.
- **Sai kích thước tệp.** Dùng đúng cặp width và height của thư mục đầu vào. Tệp `.bin` phải có đúng `width × height × 4` byte.
- **Số luồng thực tế khác yêu cầu.** Kiểm tra `OMP_THREAD_LIMIT` và cấu hình OpenMP của máy. Benchmark dừng nếu số luồng thực tế khác cấu hình.
- **Đầu ra đã tồn tại.** Chọn tên PGM hoặc tên thư mục kết quả mới. Thư mục cha phải tồn tại.
- **PowerShell chặn chạy script.** Có thể dùng các lệnh GCC trực tiếp cho từng ảnh. Để xây dựng benchmark, xem các lệnh trong `build_gcc.ps1` hoặc chạy bản WSL2.
