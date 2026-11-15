# Parallel Image Filtering with OpenMP

Phần của Người 2 gồm `sobel_seq.cpp`, `sobel_omp.cpp` và `sobel.hpp`. Hai bản dùng cùng cặp kernel Sobel 3×3 và tính biên độ gradient `sqrt(Gx² + Gy²)` kiểu `float32`. Viền ngoài một pixel được đặt bằng 0. Bản OpenMP chia các hàng bằng `schedule(static)`.

Chương trình đo dùng trực tiếp các hàm lọc Gaussian tách 1D (`19×19`, `σ = 3`, replicate padding, đầu ra `uint8`) và Laplacian 4 lân cận (zero padding, đầu ra `float32` có dấu) ở hai commit sau:

- [Gaussian của Người 1](https://github.com/ngynneyu/gaussian-1d-openmp/tree/2f639af25dd36171978e106c9f467e8a4263101d)
- [Laplacian của Người 3](https://github.com/vietquang8071/ttss/tree/91e2ed2ac05a56f2dc8bc258f60028242777b977)

## Chạy lại

Cần Git, Python 3 và GCC/G++ có hỗ trợ OpenMP. Chạy các lệnh từ thư mục gốc. Nếu đã có `data/processed/{512,1024,2048}/image_01.bin` đến `image_20.bin`, chạy thẳng hai lệnh benchmark. Mỗi lần chạy dùng một thư mục `--out` mới.

```sh
python scripts/run_benchmark.py --out results/benchmark
python scripts/summarize_benchmark.py results/benchmark
```

`run_benchmark.py` tự lấy các tệp nguồn của hai repo tại đúng commit. Trên Windows có thể truyền đường dẫn tuyệt đối đến hai trình biên dịch bằng `--cc` và `--cxx`.

Để tái tạo dataset, dùng 20 ảnh gốc từ `data/raw` trong repo Laplacian ở commit trên. Cài NumPy và Pillow cho bước này. `--numbered` đánh số theo thứ tự tên tệp. `metadata.csv` ghi tên ảnh gốc tương ứng.

```sh
python -m pip install numpy Pillow
git clone https://github.com/vietquang8071/ttss.git dataset-source
git -C dataset-source checkout --detach 91e2ed2ac05a56f2dc8bc258f60028242777b977
python scripts/preprocess.py --input dataset-source/data/raw --output data/processed --numbered
```

## Phương pháp đo

- Dùng cùng 20 ảnh grayscale ở mỗi kích thước 512×512, 1024×1024 và 2048×2048. Mỗi ảnh gốc có hai chiều tối thiểu 2048 px.
- Với mỗi ảnh và mỗi cấu hình, warm-up một lần rồi đo 10 lần, lấy median. Các cấu hình gồm bản tuần tự và OpenMP với 4, 8, 12, 16, 20 luồng. Thứ tự các mốc OpenMP xoay vòng theo ảnh.
- Đồng hồ chỉ bao quanh lời gọi hàm lọc và tính cả chi phí vùng OpenMP. Đọc ảnh, cấp phát, khởi tạo kernel, kiểm tra sai số và lưu kết quả nằm ngoài khoảng đo.
- Bảng tổng hợp lấy median của 20 median theo ảnh cho từng cấu hình. `Speedup = T_seq / T_omp`, `Efficiency (%) = 100 × Speedup / P`. Max Absolute Error và RMSE được tính theo từng pixel, rồi lấy giá trị lớn nhất trên 20 ảnh ở mỗi dòng tổng hợp.
- Lượt đo đã bàn giao chạy trên Intel Core i7-1255U với Windows 11, GCC 16.2.0 và 12 CPU logic. Hai mốc 16 và 20 luồng vượt số CPU logic. `run_config.json` lưu cờ biên dịch, biến môi trường OpenMP và SHA-256 của bộ dữ liệu đã đo.

## Tệp kết quả

| Tệp | Nội dung |
| --- | --- |
| `timings.csv` | 10 thời gian tuần tự và 10 thời gian OpenMP cho từng cặp ảnh/cấu hình, kèm sai số pixel |
| `metrics_per_image.csv` | 900 cặp so sánh với median, speedup, efficiency và correctness |
| `summary.csv` | 45 dòng tổng hợp cho ba thuật toán |
| `*_seq.bin`, `*_omp4.bin` | Đầu ra mẫu tuần tự và OpenMP 4 luồng của `image_01`, kích thước 512×512. Gaussian dùng `uint8`, Sobel và Laplacian dùng `float32` little-endian. |
