import { Link } from "react-router-dom";
import { useAuth } from "../auth/AuthContext";

export function FloatingWriteButton() {
  const { user } = useAuth();
  if (!user) return null;
  return (
    <Link to="/new" className="fab-write" title="Написать произведение" aria-label="Написать">
      <span>+</span>
    </Link>
  );
}
