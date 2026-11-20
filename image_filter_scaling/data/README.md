# Dữ liệu thực nghiệm mở rộng (mục 3.4)

Thư mục này lưu cache local cho benchmark V5 trên 100 ảnh xám 2048×2048. 20 ảnh đầu lấy từ `../../data/raw/`; 80 ảnh bổ sung là dữ liệu nguồn riêng.

- `2048_100/dataset.json` và `2048_100/manifest.csv` mô tả định dạng, checksum và nguồn của 100 ảnh; các tệp này được giữ trong repository.
- `2048_100/images.f32` chứa dữ liệu đóng gói float32 little-endian (khoảng 1,56 GiB); chỉ giữ local.
- `2048_8/` và `128_4/` là cache cục bộ cho các cấu hình khác và smoke test; file `.f32` không đưa lên GitHub.
- Khi tạo cache mới, đặt 80 ảnh bổ sung trong `images_2k/` hoặc trỏ biến `IMAGE_FILTER_SCALING_EXTRA_DATA` tới thư mục ảnh đó. Không cần đưa ảnh nguồn lên repository.

`.gitignore` loại ảnh nguồn và tệp `.f32` khỏi Git, nhưng vẫn giữ manifest và mô tả dữ liệu để đối chiếu.

Tình trạng nguồn và giấy phép của 20 ảnh gốc/80 ảnh bổ sung được ghi tại [../../data/SOURCES.md](../../data/SOURCES.md); hiện chưa có URL hoặc giấy phép cụ thể được lưu.
