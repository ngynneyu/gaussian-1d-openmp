# Nguồn và giấy phép dữ liệu ảnh

## Trạng thái hiện tại

Theo thông tin của nhóm, 20 ảnh gốc và 80 ảnh bổ sung được một thành viên tìm ngẫu nhiên trên Internet. Hồ sơ hiện có không ghi URL trang ảnh, tác giả/nhà phát hành, ngày tải hoặc điều khoản giấy phép.

`processed/metadata.csv` và manifest V5 chỉ ghi tên tệp, thông tin xử lý và/hoặc SHA-256; các trường này không xác minh quyền sử dụng.

Vì vậy, repository không gán các ảnh này vào một bộ dữ liệu, nguồn phát hành hay giấy phép Creative Commons cụ thể.

Repository không phân phối ảnh gốc, ảnh xem trước hoặc dữ liệu điểm ảnh đã tiền xử lý. Chỉ mã nguồn, metadata, manifest và kết quả thực nghiệm được lưu để phục vụ việc đối chiếu báo cáo.

Trước khi phân phối lại dữ liệu ảnh, cần xác minh nguồn phát hành và giấy phép sử dụng của từng ảnh, hoặc thay thế bằng bộ dữ liệu có giấy phép rõ ràng.

## Hồ sơ cần bổ sung

| Nhóm ảnh | Đối chiếu | Nguồn/URL | Tác giả | Giấy phép/điều khoản |
| --- | --- | --- | --- | --- |
| 20 ảnh gốc | `processed/metadata.csv`, `../results/*/run_config.json` | Chưa được lưu | Chưa xác định | Chưa xác minh |
| 80 ảnh bổ sung V5 | `../image_filter_scaling/data/2048_100/manifest.csv` | Chưa được lưu | Chưa xác định | Chưa xác minh |

Không nên suy ra giấy phép từ việc ảnh có thể tải công khai trên Internet.
