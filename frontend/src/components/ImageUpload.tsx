import { useRef, useState } from "react";
import { uploadsApi, apiErrorMessage } from "../api/client";

// Читает файл, ужимает по большей стороне до maxSize и отдаёт JPEG-Blob.
async function resizeToBlob(file: File, maxSize: number): Promise<Blob> {
  const dataUrl = await new Promise<string>((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => resolve(reader.result as string);
    reader.onerror = () => reject(reader.error);
    reader.readAsDataURL(file);
  });

  const img = await new Promise<HTMLImageElement>((resolve, reject) => {
    const image = new Image();
    image.onload = () => resolve(image);
    image.onerror = () => reject(new Error("bad image"));
    image.src = dataUrl;
  });

  let width = img.width;
  let height = img.height;
  if (width > maxSize || height > maxSize) {
    const scale = Math.min(maxSize / width, maxSize / height);
    width = Math.round(width * scale);
    height = Math.round(height * scale);
  }

  const canvas = document.createElement("canvas");
  canvas.width = width;
  canvas.height = height;
  const ctx = canvas.getContext("2d");
  if (!ctx) throw new Error("no canvas");
  ctx.fillStyle = "#ffffff";
  ctx.fillRect(0, 0, width, height);
  ctx.drawImage(img, 0, 0, width, height);

  return await new Promise<Blob>((resolve, reject) => {
    canvas.toBlob(
      (blob) => (blob ? resolve(blob) : reject(new Error("no blob"))),
      "image/jpeg",
      0.85,
    );
  });
}

export function ImageUpload({
  value,
  onChange,
  maxSize = 1280,
  shape = "rect",
}: {
  value: string;
  onChange: (url: string) => void;
  maxSize?: number;
  shape?: "rect" | "circle";
}) {
  const inputRef = useRef<HTMLInputElement>(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState("");

  const onFile = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;
    setBusy(true);
    setError("");
    try {
      const blob = await resizeToBlob(file, maxSize);
      const url = await uploadsApi.upload(blob);
      onChange(url);
    } catch (err) {
      setError(apiErrorMessage(err));
    } finally {
      setBusy(false);
      if (inputRef.current) inputRef.current.value = "";
    }
  };

  return (
    <div className="image-upload">
      <div className={`image-upload-preview ${shape === "circle" ? "is-circle" : ""}`}>
        {value ? <img src={value} alt="" /> : <span className="muted">нет файла</span>}
      </div>
      <div className="image-upload-actions">
        <button
          type="button"
          className="btn btn-soft btn-sm"
          onClick={() => inputRef.current?.click()}
          disabled={busy}
        >
          {busy ? "Загрузка…" : value ? "Заменить" : "Загрузить файл"}
        </button>
        {value && (
          <button
            type="button"
            className="btn btn-soft btn-sm"
            onClick={() => onChange("")}
          >
            Убрать
          </button>
        )}
        <input ref={inputRef} type="file" accept="image/*" hidden onChange={onFile} />
        {error && <span className="error">{error}</span>}
      </div>
    </div>
  );
}
